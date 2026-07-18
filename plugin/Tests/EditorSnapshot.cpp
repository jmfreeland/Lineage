#include "../Source/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

#include <memory>

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI initialiseJuce;
  LineageAudioProcessor processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
  editor->setSize(1240, 760);
  if (argc > 2) {
    if (auto* tabs = dynamic_cast<juce::TabbedComponent*>(editor->findChildWithID("workspaceTabs"))) {
      tabs->setCurrentTabIndex(juce::String(argv[2]).getIntValue());
    }
  }

  juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
  juce::Graphics graphics(image);
  editor->paintEntireComponent(graphics, true);

  const auto destination = argc > 1 ? juce::File(argv[1])
                                    : juce::File::getSpecialLocation(juce::File::tempDirectory)
                                          .getChildFile("lineage-editor.png");
  destination.deleteFile();
  auto stream = destination.createOutputStream();
  if (stream == nullptr) return 1;

  juce::PNGImageFormat format;
  return format.writeImageToStream(image, *stream) ? 0 : 1;
}
