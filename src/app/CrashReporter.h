#pragma once
// =============================================================================
//  CrashReporter (Fase 9): si el programa se cierra de golpe, guarda un informe
//  (logs/crash-AAAAMMDD-HHMMSS.txt) con la pila de llamadas, el sistema y el
//  final del log. Al abrir de nuevo, ofrece ver la carpeta del informe para
//  mandarlo a soporte (el envío automático queda preparado: supportEmail()).
// =============================================================================
#include <juce_core/juce_core.h>

namespace kb::app::crash
{
    /** Instalar al arrancar (lo antes posible). */
    void install (const juce::File& logsFolder);
    /** Informes nuevos desde la última vez que se mostraron. */
    juce::Array<juce::File> newReports (const juce::File& logsFolder, juce::int64 lastSeenMs);
    /** Dirección de soporte para mandar informes (vacía = solo se abre la carpeta). */
    juce::String supportEmail();
} // namespace kb::app::crash
