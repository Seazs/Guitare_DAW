#include "TunerComponent.h"

void TunerMeter::setCents(int cents)
{
    displayedCents = juce::jlimit(-50, 50, cents);
    repaint();
}

void TunerMeter::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colour(0xff303947));
    g.fillRoundedRectangle(area, 4.0f);

    g.setColour(juce::Colour(0xff744c56));
    g.fillRect(area.getX(), area.getY(), area.getWidth() * 0.32f, area.getHeight());
    g.fillRect(area.getRight() - area.getWidth() * 0.32f, area.getY(),
               area.getWidth() * 0.32f, area.getHeight());

    g.setColour(juce::Colour(0xffb89a4a));
    g.fillRect(area.getX() + area.getWidth() * 0.32f, area.getY(),
               area.getWidth() * 0.18f, area.getHeight());
    g.fillRect(area.getRight() - area.getWidth() * 0.50f, area.getY(),
               area.getWidth() * 0.18f, area.getHeight());

    g.setColour(juce::Colour(0xff65d89b));
    g.fillRoundedRectangle(area.withWidth(area.getWidth() * 0.36f)
                               .withCentre(juce::Point<float>(area.getCentreX(), area.getCentreY())),
                           3.0f);

    const float normalised = juce::jlimit(-1.0f, 1.0f, (float) displayedCents / 50.0f);
    const float markerX = area.getX() + area.getWidth() * (0.5f + normalised * 0.5f);
    g.setColour(juce::Colour(0xfff4f7fa));
    g.fillRoundedRectangle(markerX - 2.0f, area.getY() - 3.0f, 4.0f, area.getHeight() + 6.0f, 2.0f);

    g.setColour(juce::Colour(0xff11151b));
    g.fillRect(area.getCentreX() - 0.5f, area.getY(), 1.0f, area.getHeight());
}

TunerComponent::TunerComponent()
{
    ringBuffer.assign(8192, 0.0f);

    noteLabel.setText("-", juce::dontSendNotification);
    noteLabel.setJustificationType(juce::Justification::centred);
    noteLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    noteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(noteLabel);

    centsLabel.setText("-- cents", juce::dontSendNotification);
    centsLabel.setJustificationType(juce::Justification::centred);
    centsLabel.setFont(juce::Font(13.0f));
    centsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaeb9c8));
    addAndMakeVisible(centsLabel);

    statusLabel.setText("Waiting", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8e9aaa));
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(meter);

    startTimerHz(10);
}

TunerComponent::~TunerComponent()
{
    stopTimer();
}

void TunerComponent::prepareToPlay(double sampleRateToUse)
{
    sampleRate = sampleRateToUse;
    fifo.reset();
    ringBuffer.assign(8192, 0.0f);
}

void TunerComponent::pushSamples(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    const int numToWrite = juce::jmin(numSamples, fifo.getFreeSpace());
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite(numToWrite, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        ringBuffer[(size_t) start1 + i] = buffer.getSample(0, startSample + i);

    for (int i = 0; i < size2; ++i)
        ringBuffer[(size_t) start2 + i] = buffer.getSample(0, startSample + size1 + i);

    fifo.finishedWrite(numToWrite);
}

void TunerComponent::timerCallback()
{
    updatePitch();
}

void TunerComponent::updatePitch()
{
    const int available = fifo.getNumReady();
    if (available < 4096)
        return;

    const int skip = available - 4096;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    if (skip > 0)
    {
        fifo.prepareToRead(skip, start1, size1, start2, size2);
        fifo.finishedRead(skip);
    }

    std::array<float, 4096> samples {};
    start1 = size1 = start2 = size2 = 0;
    fifo.prepareToRead(4096, start1, size1, start2, size2);

    int outputIndex = 0;
    for (int i = 0; i < size1; ++i)
        samples[outputIndex++] = ringBuffer[(size_t) start1 + i];
    for (int i = 0; i < size2; ++i)
        samples[outputIndex++] = ringBuffer[(size_t) start2 + i];

    fifo.finishedRead(4096);

    double rms = 0.0;
    for (float sample : samples)
        rms += sample * sample;
    rms = std::sqrt(rms / samples.size());

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (rms < 0.008)
    {
        if (now - pitchLastSeen > 650.0)
        {
            hasPitch = false;
            smoothedCents = 0.0;
            meter.setCents(0);
            noteLabel.setText("-", juce::dontSendNotification);
            centsLabel.setText("-- cents", juce::dontSendNotification);
            statusLabel.setText("Waiting", juce::dontSendNotification);
            noteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        return;
    }

    const int minLag = juce::jmax(2, (int) std::round(sampleRate / 1000.0));
    const int maxLag = juce::jmin(735, (int) std::round(sampleRate / 60.0));
    double runningDifference = 0.0;
    double bestDifference = 1.0e9;
    int bestLag = -1;

    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double difference = 0.0;
        for (int i = 0; i < 4096 - lag; ++i)
        {
            const double delta = samples[i] - samples[i + lag];
            difference += delta * delta;
        }

        runningDifference += difference;
        const double yin = lag * difference / juce::jmax(runningDifference, 1.0e-12);
        if (lag >= minLag && yin < 0.22 && yin < bestDifference)
        {
            bestDifference = yin;
            bestLag = lag;
        }
    }

    if (bestLag < minLag)
        return;

    const double frequency = sampleRate / bestLag;
    if (frequency < 60.0 || frequency > 1000.0)
        return;

    const double midiFloat = 69.0 + 12.0 * std::log2(frequency / 440.0);
    const int nearestMidi = (int) std::round(midiFloat);
    const double nearestFrequency = 440.0 * std::pow(2.0, (nearestMidi - 69) / 12.0);
    const int cents = (int) std::round(1200.0 * std::log2(frequency / nearestFrequency));

    pitchLastSeen = now;
    smoothedCents = hasPitch ? smoothedCents * 0.55 + cents * 0.45
                             : (double) cents;
    const int displayCents = (int) std::round(smoothedCents);

    static constexpr const char* noteNames[] =
    {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    hasPitch = true;
    const int noteIndex = ((nearestMidi % 12) + 12) % 12;
    const int octave = nearestMidi / 12 - 1;
    noteLabel.setText(juce::String(noteNames[noteIndex]) + juce::String(octave),
                      juce::dontSendNotification);
    centsLabel.setText((displayCents >= 0 ? "+" : "") + juce::String(displayCents) + " cents",
                       juce::dontSendNotification);
    meter.setCents(displayCents);

    if (std::abs(displayCents) <= 5)
        statusLabel.setText("In tune", juce::dontSendNotification);
    else if (displayCents < 0)
        statusLabel.setText("Flat", juce::dontSendNotification);
    else
        statusLabel.setText("Sharp", juce::dontSendNotification);

    noteLabel.setColour(juce::Label::textColourId,
                        std::abs(displayCents) <= 5 ? juce::Colour(0xff65d89b)
                                                    : juce::Colours::white);
}

void TunerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        juce::Colour(0xff252d38), bounds.getX(), bounds.getY(),
        juce::Colour(0xff1a2029), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff3c4858));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    g.setColour(juce::Colour(0xff8e9aaa));
    g.setFont(10.0f);
    g.drawText("Tuner", 0, 8, getWidth(), 14,
               juce::Justification::centred, false);
}

void TunerComponent::resized()
{
    auto bounds = getLocalBounds();
    noteLabel.setBounds(bounds.removeFromTop(52).reduced(8, 2));
    centsLabel.setBounds(bounds.removeFromTop(24).reduced(8, 2));
    statusLabel.setBounds(bounds.removeFromTop(18).reduced(8, 1));
    meter.setBounds(bounds.removeFromTop(20).reduced(8, 2));
}
