#include "SoundFontManager.h"

SoundFontManager::SoundFontManager (FluidSynthEngine& e) : engine (e) {}

bool SoundFontManager::loadFile (const juce::File& sf2)
{
    if (!sf2.existsAsFile() || sf2.getFileExtension().toLowerCase() != ".sf2")
        return false;

    if (engine.loadSoundFont (sf2))
    {
        currentFile = sf2;
        return true;
    }
    return false;
}

void SoundFontManager::openFileChooser (juce::Component* parent,
                                         std::function<void(bool)> callback)
{
    chooser = std::make_unique<juce::FileChooser> (
        "Selecione um SoundFont (.sf2)",
        currentFile.existsAsFile() ? currentFile.getParentDirectory()
                                   : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.sf2");

    chooser->launchAsync (juce::FileBrowserComponent::openMode |
                          juce::FileBrowserComponent::canSelectFiles,
        [this, callback] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
                callback (loadFile (result));
            else
                callback (false);
        });
}

void SoundFontManager::saveState (juce::PropertiesFile& props) const
{
    if (currentFile.existsAsFile())
        props.setValue ("lastSoundFont", currentFile.getFullPathName());
}

void SoundFontManager::restoreState (juce::PropertiesFile& props)
{
    auto path = props.getValue ("lastSoundFont");
    if (path.isNotEmpty())
        loadFile (juce::File (path));
}
