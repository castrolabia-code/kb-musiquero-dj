// Punto de entrada de las pruebas: inicializa JUCE (sin ventanas) y corre Catch2.
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // MessageManager para utilidades de JUCE
    const int result = Catch::Session().run (argc, argv);
    juce::LocalisedStrings::setCurrentMappings (nullptr);
    return result;
}
