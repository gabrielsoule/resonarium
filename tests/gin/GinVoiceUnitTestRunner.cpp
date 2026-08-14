#include <gin_dsp/gin_dsp.h>
#include <gin_plugin/gin_plugin.h>

#include <iostream>

using namespace gin;

#include <gin_dsp/dsp/gin_synthesiser.test.h>
#include <gin_plugin/plugin/gin_modmatrix.test.h>

namespace
{
class ConsoleUnitTestRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << '\n';
    }
};
}

int main()
{
    juce::Array<juce::UnitTest*> selectedTests;

    for (auto* test : juce::UnitTest::getAllTests())
    {
        if (test->getCategory() == "gin_dsp" || test->getCategory() == "gin_plugin")
            selectedTests.add (test);
    }

    if (selectedTests.size() != 2)
    {
        std::cerr << "Expected exactly two Gin voice-system test suites, found "
                  << selectedTests.size() << '\n';
        return 1;
    }

    ConsoleUnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTests (selectedTests, 0x5245534f4e415249LL);

    int passes = 0;
    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult (i))
        {
            passes += result->passes;
            failures += result->failures;
        }
    }

    std::cout << "Gin voice-system summary: " << runner.getNumResults() << " cases, "
              << passes << " checks passed, " << failures << " checks failed\n";

    return failures == 0 && runner.getNumResults() != 0 ? 0 : 1;
}
