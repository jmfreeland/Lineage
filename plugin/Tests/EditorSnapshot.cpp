#include "../Source/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

#include <memory>

namespace {
juce::Component* findDescendantWithID(juce::Component& parent, const juce::String& componentID) {
  if (parent.getComponentID() == componentID) return &parent;
  for (int index = 0; index < parent.getNumChildComponents(); ++index) {
    if (auto* match = findDescendantWithID(*parent.getChildComponent(index), componentID)) return match;
  }
  return nullptr;
}
} // namespace

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
  if (argc > 3) {
    if (auto* presetTabs = dynamic_cast<juce::TabbedComponent*>(findDescendantWithID(*editor, "libraryPresetTabs"))) {
      presetTabs->setCurrentTabIndex(juce::String(argv[3]).getIntValue());
    }
  }
  if (argc > 4) {
    const auto action = juce::String(argv[4]);
    const auto buttonID = action == "branch" ? "branchButton"
        : action == "start" ? "startEvolutionButton"
                            : "evolveButton";
    if (auto* button = dynamic_cast<juce::Button*>(findDescendantWithID(*editor, buttonID))) {
      if (action == "start") button->setToggleState(true, juce::dontSendNotification);
      if (button->onClick != nullptr) button->onClick();
      if (action == "start") {
        juce::Thread::sleep(220);
        juce::Timer::callPendingTimersSynchronously();
      }
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
