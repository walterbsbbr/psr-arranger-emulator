#pragma once
#include <JuceHeader.h>
#include "FluidSynthEngine.h"

/**
 * SoundFontManager
 *
 * Gerencia a seleção e carregamento de arquivos .sf2.
 * Persiste o caminho do último SF2 via juce::PropertiesFile.
 */
class SoundFontManager
{
public:
    explicit SoundFontManager (FluidSynthEngine& engine);

    // Abre diálogo de seleção de arquivo .sf2
    void openFileChooser (juce::Component* parent,
                          std::function<void(bool loaded)> callback);

    // Carrega diretamente (usado para restaurar sessão anterior)
    bool loadFile (const juce::File& sf2);

    juce::File  getCurrentFile()  const { return currentFile; }
    juce::String getCurrentName() const { return engine.getLoadedSoundFontName(); }
    bool         isLoaded()       const { return engine.isSoundFontLoaded(); }

    // Salva/restaura último SF2 usado
    void saveState (juce::PropertiesFile& props) const;
    void restoreState (juce::PropertiesFile& props);

private:
    FluidSynthEngine& engine;
    juce::File        currentFile;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFontManager)
};
