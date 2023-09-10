#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Editor::Editor (Processor& p)
    : gin::ProcessorEditor (p), ginProc (p)
{
    setGridSize (10, 3);
}

Editor::~Editor()
{
}

//==============================================================================

void Editor::resized()
{
    ProcessorEditor::resized ();
}

