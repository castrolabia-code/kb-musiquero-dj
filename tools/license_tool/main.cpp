// =============================================================================
//  kb_license_tool: herramienta de Castro Labs para las licencias.
//
//    kb_license_tool genkeys <carpeta>
//        Crea un par de claves RSA 2048: private.key (GUARDAR EN SECRETO) y
//        public.key (va dentro del programa: src/licensing/PublicKey.h).
//
//    kb_license_tool issue <private.key> "<Nombre>" <email> [--expires AAAA-MM-DD] [--machine ID] [--edition Pro]
//        Imprime una clave de producto firmada para un cliente.
//
//    kb_license_tool verify <public.key> <clave>
// =============================================================================
#include "licensing/License.h"

#include <iostream>

using namespace kb::licensing;

static int usage()
{
    std::cout << "Uso:\n"
                 "  kb_license_tool genkeys <carpeta>\n"
                 "  kb_license_tool issue <private.key> \"<Nombre>\" <email> [--expires AAAA-MM-DD] [--machine ID] [--edition Pro]\n"
                 "  kb_license_tool verify <public.key> <clave>\n";
    return 1;
}

int main (int argc, char** argv)
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));
    if (args.isEmpty())
        return usage();

    if (args[0] == "genkeys" && args.size() >= 2)
    {
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]));
        dir.createDirectory();
        juce::RSAKey pub, priv;
        std::cout << "Generando claves RSA 2048 (puede tardar unos segundos)...\n";
        juce::RSAKey::createKeyPair (pub, priv, 2048);
        dir.getChildFile ("public.key").replaceWithText (pub.toString());
        dir.getChildFile ("private.key").replaceWithText (priv.toString());
        std::cout << "Listo: " << dir.getFullPathName() << "\n"
                  << "  private.key: GUARDALA EN SECRETO (con ella se fabrican licencias).\n"
                  << "  public.key:  copiala en src/licensing/PublicKey.h\n";
        return 0;
    }

    if (args[0] == "issue" && args.size() >= 4)
    {
        const juce::RSAKey priv (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]).loadFileAsString().trim());
        if (! priv.isValid())
        {
            std::cerr << "Clave privada inválida\n";
            return 2;
        }
        LicenseInfo info;
        info.name = args[2];
        info.email = args[3];
        info.issuedMs = juce::Time::currentTimeMillis();
        info.id = juce::Uuid().toString().substring (0, 12).toUpperCase();
        for (int i = 4; i + 1 < args.size(); i += 2)
        {
            if (args[i] == "--expires")
            {
                const auto p = juce::StringArray::fromTokens (args[i + 1], "-", "");
                if (p.size() == 3)
                    info.expiresMs = juce::Time (p[0].getIntValue(), p[1].getIntValue() - 1, p[2].getIntValue(), 23, 59).toMilliseconds();
            }
            else if (args[i] == "--machine") info.machine = args[i + 1].toUpperCase();
            else if (args[i] == "--edition") info.edition = args[i + 1];
        }
        std::cout << signLicense (info, priv) << "\n";
        return 0;
    }

    if (args[0] == "verify" && args.size() >= 3)
    {
        const juce::RSAKey pub (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]).loadFileAsString().trim());
        const auto r = verifyLicense (args[2], pub, machineId());
        std::cout << statusKey (r.status) << "  " << r.info.name << " <" << r.info.email << ">\n";
        return r.isValid() ? 0 : 3;
    }
    return usage();
}
