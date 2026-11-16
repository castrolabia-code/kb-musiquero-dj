// =============================================================================
//  KB Generador de Licencias — herramienta INTERNA de Castro Labs.
//
//  Programa con ventana para fabricar claves de producto de KB - MUSIQUERO DJ
//  sin usar la consola. NO se entrega a los clientes (no va en el instalador).
//
//  * Carga la clave PRIVADA (private.key) y comprueba que corresponde a la
//    clave pública que trae el programa (si no, las licencias no servirían).
//  * Datos del cliente, vencimiento (perpetua / meses / fecha) y atadura
//    opcional a una PC (ID que el cliente ve en la ventana de activación).
//  * Genera la clave, la verifica, la copia o la guarda en un .txt listo para
//    mandar, y la anota en un historial CSV (Documentos/KB Licencias).
//  * Verifica claves existentes (para responder consultas de soporte).
// =============================================================================
#include "licensing/License.h"

#include <KbBinaryData.h>
#include <juce_gui_extra/juce_gui_extra.h>

using namespace kb::licensing;

namespace
{
    const juce::Colour yellow (0xfff5d90a), panel (0xff1e2126), bg (0xff121417), cyan (0xff19e3f5),
                       green (0xff2bea5a), red (0xffff4a3d), text (0xffe8eaed), dim (0xff9aa0a8);

    juce::File dataFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KB Licencias");
    }

    juce::String dateText (juce::int64 ms)
    {
        return ms == 0 ? juce::String ("perpetua") : juce::Time (ms).formatted ("%Y-%m-%d");
    }

    juce::String csvField (juce::String s)
    {
        s = s.replace ("\"", "\"\"");
        return "\"" + s + "\"";
    }

    // ---------------------------------------------------------------- estilo
    struct Look final : juce::LookAndFeel_V4
    {
        Look()
        {
            regular = juce::Typeface::createSystemTypefaceFor (KbBinaryData::RajdhaniSemiBold_ttf, (size_t) KbBinaryData::RajdhaniSemiBold_ttfSize);
            bold    = juce::Typeface::createSystemTypefaceFor (KbBinaryData::RajdhaniBold_ttf, (size_t) KbBinaryData::RajdhaniBold_ttfSize);
            title   = juce::Typeface::createSystemTypefaceFor (KbBinaryData::OrbitronBlack_ttf, (size_t) KbBinaryData::OrbitronBlack_ttfSize);

            setColour (juce::ResizableWindow::backgroundColourId, bg);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0c0e));
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3f47));
            setColour (juce::TextEditor::focusedOutlineColourId, cyan);
            setColour (juce::TextEditor::highlightColourId, cyan.withAlpha (0.35f));
            setColour (juce::CaretComponent::caretColourId, cyan);
            setColour (juce::Label::textColourId, text);
            setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0b0c0e));
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f47));
            setColour (juce::ComboBox::arrowColourId, yellow);
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, yellow);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e35));
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0b0c0e));
            setColour (juce::AlertWindow::backgroundColourId, panel);
            setColour (juce::AlertWindow::textColourId, text);
        }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == "Orbitron") return title;
            return f.isBold() ? bold : regular;
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override
        {
            return juce::Font (juce::FontOptions ((float) h * 0.5f, juce::Font::bold));
        }

        juce::Typeface::Ptr regular, bold, title;
    };

    // ---------------------------------------------------------------- historial
    struct HistoryModel final : juce::ListBoxModel
    {
        juce::StringArray rows;   // "fecha · nombre · email · vence · id"

        int getNumRows() override { return rows.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (selected)
                g.fillAll (yellow.withAlpha (0.18f));
            else if (row % 2)
                g.fillAll (juce::Colours::white.withAlpha (0.03f));
            g.setColour (text);
            g.setFont (juce::FontOptions ((float) h * 0.62f));
            g.drawText (rows[row], 8, 0, w - 16, h, juce::Justification::centredLeft, true);
        }

        void load()
        {
            rows.clear();
            juce::StringArray lines;
            lines.addLines (dataFolder().getChildFile ("licencias_emitidas.csv").loadFileAsString());
            for (int i = lines.size(); --i >= 1;)   // la línea 0 es el encabezado; las más nuevas arriba
            {
                juce::StringArray f;
                f.addTokens (lines[i], ",", "\"");
                f.trim();
                f.removeEmptyStrings();
                for (auto& s : f)
                    s = s.unquoted();
                if (f.size() >= 5)
                    rows.add (f[0] + juce::String::fromUTF8 ("   ·   ") + f[1] + juce::String::fromUTF8 ("   ·   ") + f[2] + juce::String::fromUTF8 ("   ·   vence: ") + f[4] + juce::String::fromUTF8 ("   ·   ID ") + f[3]);
            }
        }
    };

    // ---------------------------------------------------------------- ventana principal
    class GeneratorComponent final : public juce::Component
    {
    public:
        explicit GeneratorComponent (const juce::File& keyFromCommandLine)
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "KB Generador de Licencias";
            o.filenameSuffix = ".settings";
            o.folderName = "Castro Labs";
            o.osxLibrarySubFolder = "Application Support";
            settings = std::make_unique<juce::PropertiesFile> (o);

            for (auto* l : { &keyTitle, &clientTitle, &resultTitle, &verifyTitle, &historyTitle })
            {
                l->setFont (juce::FontOptions ("Orbitron", 15.0f, juce::Font::plain));
                l->setColour (juce::Label::textColourId, yellow);
                addAndMakeVisible (*l);
            }
            keyTitle.setText (juce::String::fromUTF8 ("1 · CLAVE PRIVADA"), juce::dontSendNotification);
            clientTitle.setText (juce::String::fromUTF8 ("2 · DATOS DEL CLIENTE"), juce::dontSendNotification);
            resultTitle.setText (juce::String::fromUTF8 ("3 · CLAVE DE PRODUCTO"), juce::dontSendNotification);
            verifyTitle.setText ("VERIFICAR UNA CLAVE", juce::dontSendNotification);
            historyTitle.setText ("LICENCIAS EMITIDAS", juce::dontSendNotification);

            addAndMakeVisible (keyStatus);
            keyStatus.setFont (juce::FontOptions (17.0f));
            addAndMakeVisible (chooseKey);
            chooseKey.setButtonText ("Elegir private.key...");
            chooseKey.onClick = [this] { chooseKeyFile(); };

            auto field = [this] (juce::Label& label, const juce::String& caption, juce::TextEditor& editor, const juce::String& hint)
            {
                label.setText (caption, juce::dontSendNotification);
                label.setFont (juce::FontOptions (17.0f));
                label.setColour (juce::Label::textColourId, dim);
                addAndMakeVisible (label);
                editor.setTextToShowWhenEmpty (hint, dim.withAlpha (0.6f));
                editor.setFont (juce::FontOptions (19.0f));
                editor.setIndents (8, 6);
                addAndMakeVisible (editor);
            };
            field (nameLabel, "Nombre del cliente *", nameEdit, juce::String::fromUTF8 ("Ej.: Juan Pérez / Disco Bar La Esquina"));
            field (emailLabel, "Email *", emailEdit, "cliente@correo.com");
            field (dateLabel, "Fecha de vencimiento", dateEdit, "AAAA-MM-DD");
            field (machineLabel, "ID de la PC (opcional)", machineEdit, juce::String::fromUTF8 ("Vacío = cualquier PC"));
            machineEdit.setInputRestrictions (16, "0123456789abcdefABCDEF");

            editionLabel.setText (juce::String::fromUTF8 ("Edición"), juce::dontSendNotification);
            editionLabel.setFont (juce::FontOptions (17.0f));
            editionLabel.setColour (juce::Label::textColourId, dim);
            addAndMakeVisible (editionLabel);
            edition.addItemList ({ "Pro", "Estudio", "Academia" }, 1);
            edition.setEditableText (true);
            edition.setSelectedId (1);
            addAndMakeVisible (edition);

            expiryLabel.setText (juce::String::fromUTF8 ("Duración"), juce::dontSendNotification);
            expiryLabel.setFont (juce::FontOptions (17.0f));
            expiryLabel.setColour (juce::Label::textColourId, dim);
            addAndMakeVisible (expiryLabel);
            expiry.addItemList (juce::StringArray { juce::String ("Perpetua (no vence)"), "1 mes", "3 meses", "6 meses", juce::String::fromUTF8 ("1 año"), juce::String ("Hasta una fecha...") }, 1);
            expiry.setSelectedId (1);
            expiry.onChange = [this] { dateEdit.setEnabled (expiry.getSelectedId() == 6); dateEdit.setAlpha (dateEdit.isEnabled() ? 1.0f : 0.4f); };
            expiry.onChange();
            addAndMakeVisible (expiry);

            machineHelp.setText (juce::String::fromUTF8 ("El cliente ve su ID en la ventana de activación (botón \"Copiar ID\"). "
                                                         "Si lo ponés, la clave solo funciona en esa PC."), juce::dontSendNotification);
            machineHelp.setFont (juce::FontOptions (15.0f));
            machineHelp.setColour (juce::Label::textColourId, dim);
            addAndMakeVisible (machineHelp);

            generate.setButtonText ("GENERAR LICENCIA");
            generate.setColour (juce::TextButton::buttonColourId, yellow);
            generate.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
            generate.onClick = [this] { generateKey(); };
            addAndMakeVisible (generate);

            result.setReadOnly (true);
            result.setMultiLine (true, true);
            result.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
            result.setColour (juce::TextEditor::textColourId, cyan);
            result.setTextToShowWhenEmpty (juce::String::fromUTF8 ("La clave aparece acá."), dim.withAlpha (0.6f));
            addAndMakeVisible (result);
            addAndMakeVisible (resultInfo);
            resultInfo.setFont (juce::FontOptions (17.0f));

            copyKey.setButtonText ("Copiar clave");
            copyKey.onClick = [this] { if (lastKey.isNotEmpty()) { juce::SystemClipboard::copyTextToClipboard (lastKey); flash ("Clave copiada."); } };
            copyMessage.setButtonText ("Copiar mensaje para el cliente");
            copyMessage.onClick = [this] { if (lastKey.isNotEmpty()) { juce::SystemClipboard::copyTextToClipboard (customerMessage()); flash ("Mensaje copiado: pegalo en el mail o WhatsApp."); } };
            saveTxt.setButtonText ("Guardar .txt");
            saveTxt.onClick = [this] { saveForCustomer(); };
            for (auto* b : { &copyKey, &copyMessage, &saveTxt })
            {
                b->setEnabled (false);
                addAndMakeVisible (*b);
            }

            verifyEdit.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Pegá acá una clave KBDJ1-... para ver de quién es y si es válida"), dim.withAlpha (0.6f));
            verifyEdit.setFont (juce::FontOptions (16.0f));
            verifyEdit.setIndents (8, 6);
            verifyEdit.onReturnKey = [this] { verifyKey(); };
            addAndMakeVisible (verifyEdit);
            verifyButton.setButtonText ("Verificar");
            verifyButton.onClick = [this] { verifyKey(); };
            addAndMakeVisible (verifyButton);
            addAndMakeVisible (verifyResult);
            verifyResult.setFont (juce::FontOptions (17.0f));

            history.setModel (&historyModel);
            history.setRowHeight (26);
            addAndMakeVisible (history);
            openFolder.setButtonText ("Abrir carpeta");
            openFolder.onClick = [] { dataFolder().createDirectory(); dataFolder().startAsProcess(); };
            addAndMakeVisible (openFolder);
            historyModel.load();
            history.updateContent();

            // Se puede abrir arrastrando el private.key sobre el .exe (o pasándolo como argumento).
            loadKey (keyFromCommandLine.existsAsFile() ? keyFromCommandLine : juce::File (settings->getValue ("privateKey")));
            setSize (980, 960);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (bg);
            // Barra superior amarilla como el programa.
            auto top = getLocalBounds().removeFromTop (78).toFloat();
            g.setGradientFill (juce::ColourGradient (yellow.brighter (0.15f), 0, 0, yellow.darker (0.12f), 0, top.getBottom(), false));
            g.fillRect (top);
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions ("Orbitron", 30.0f, juce::Font::plain));
            g.drawText ("KB MUSIQUERO DJ", top.reduced (24, 0).removeFromTop (52), juce::Justification::bottomLeft);
            g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
            g.drawText (juce::String::fromUTF8 ("GENERADOR DE LICENCIAS  ·  USO EXCLUSIVO DE CASTRO LABS"), top.reduced (26, 0).removeFromBottom (26),
                        juce::Justification::topLeft);
            g.setFont (juce::FontOptions ("Orbitron", 16.0f, juce::Font::plain));
            g.drawText ("BY CASTRO LABS", top.reduced (24, 0), juce::Justification::centredRight);

            for (auto r : panels)
            {
                g.setColour (panel);
                g.fillRoundedRectangle (r.toFloat(), 8.0f);
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
            }
        }

        void resized() override
        {
            panels.clear();
            auto area = getLocalBounds().reduced (16);
            area.removeFromTop (78);
            area.removeFromTop (4);

            auto section = [&] (int h)
            {
                auto r = area.removeFromTop (h);
                area.removeFromTop (10);
                panels.add (r);
                return r.reduced (14, 10);
            };

            // 1 · clave privada
            {
                auto r = section (82);
                keyTitle.setBounds (r.removeFromTop (22));
                r.removeFromTop (6);
                chooseKey.setBounds (r.removeFromRight (210).withHeight (32));
                r.removeFromRight (10);
                keyStatus.setBounds (r.withHeight (32));
            }
            // 2 · datos del cliente
            {
                auto r = section (250);
                clientTitle.setBounds (r.removeFromTop (22));
                r.removeFromTop (6);
                auto row = [&] (juce::Component& a, juce::Component& ae, juce::Component& b, juce::Component& be)
                {
                    auto line = r.removeFromTop (58);
                    r.removeFromTop (4);
                    auto left = line.removeFromLeft (line.getWidth() / 2 - 8);
                    line.removeFromLeft (16);
                    a.setBounds (left.removeFromTop (22));
                    ae.setBounds (left.withHeight (34));
                    b.setBounds (line.removeFromTop (22));
                    be.setBounds (line.withHeight (34));
                };
                row (nameLabel, nameEdit, emailLabel, emailEdit);
                row (editionLabel, edition, expiryLabel, expiry);
                auto line = r.removeFromTop (58);
                auto left = line.removeFromLeft (line.getWidth() / 2 - 8);
                line.removeFromLeft (16);
                machineLabel.setBounds (left.removeFromTop (22));
                machineEdit.setBounds (left.withHeight (34));
                dateLabel.setBounds (line.removeFromTop (22));
                dateEdit.setBounds (line.withHeight (34));
                machineHelp.setBounds (r.removeFromTop (24));
            }
            // botón generar
            generate.setBounds (area.removeFromTop (50).reduced (220, 0));
            area.removeFromTop (10);
            // 3 · resultado
            {
                auto r = section (196);
                resultTitle.setBounds (r.removeFromTop (22));
                r.removeFromTop (6);
                auto buttons = r.removeFromBottom (34);
                r.removeFromBottom (6);
                resultInfo.setBounds (r.removeFromBottom (26));
                result.setBounds (r);
                saveTxt.setBounds (buttons.removeFromRight (150));
                buttons.removeFromRight (10);
                copyMessage.setBounds (buttons.removeFromRight (280));
                buttons.removeFromRight (10);
                copyKey.setBounds (buttons.removeFromRight (160));
            }
            // verificar
            {
                auto r = section (100);
                verifyTitle.setBounds (r.removeFromTop (22));
                r.removeFromTop (4);
                auto line = r.removeFromTop (34);
                verifyButton.setBounds (line.removeFromRight (130));
                line.removeFromRight (10);
                verifyEdit.setBounds (line);
                verifyResult.setBounds (r);
            }
            // historial
            {
                auto r = section (juce::jmax (90, area.getHeight()));
                auto head = r.removeFromTop (24);
                openFolder.setBounds (head.removeFromRight (140).withHeight (24));
                historyTitle.setBounds (head);
                r.removeFromTop (6);
                history.setBounds (r);
            }
        }

    private:
        // ------------------------------------------------------------ clave privada
        void chooseKeyFile()
        {
            chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Elegí la clave privada de licencias (private.key)"),
                                                           juce::File (settings->getValue ("privateKey")).getParentDirectory(),
                                                           "*.key;*.txt");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (fc.getResult().existsAsFile())
                                          loadKey (fc.getResult());
                                  });
        }

        void loadKey (const juce::File& file)
        {
            privateKey = {};
            keyOk = false;
            if (! file.existsAsFile())
            {
                setKeyStatus (juce::String::fromUTF8 ("Elegí el archivo de la clave privada para empezar."), dim);
                updateEnabled();
                return;
            }
            const juce::RSAKey key (file.loadFileAsString().trim());
            if (! key.isValid())
            {
                setKeyStatus ("\"" + file.getFileName() + juce::String::fromUTF8 ("\" no es una clave privada válida."), red);
                updateEnabled();
                return;
            }
            // ¿Corresponde a la clave pública que trae el programa? Se firma una licencia de
            // prueba y se verifica como lo haría KB Musiquero DJ.
            LicenseInfo probe;
            probe.id = "PRUEBA";
            probe.name = "prueba";
            probe.email = "prueba@prueba";
            probe.issuedMs = juce::Time::currentTimeMillis();
            const auto ok = verifyLicense (signLicense (probe, key), productPublicKey(), machineId()).isValid();
            if (! ok)
            {
                setKeyStatus (juce::String::fromUTF8 ("Esta clave NO corresponde a KB Musiquero DJ: sus licencias no funcionarían."), red);
                updateEnabled();
                return;
            }
            privateKey = key;
            keyOk = true;
            settings->setValue ("privateKey", file.getFullPathName());
            settings->saveIfNeeded();
            setKeyStatus (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 ")) + "Lista: " + file.getFullPathName(), green);
            updateEnabled();
        }

        void setKeyStatus (const juce::String& s, juce::Colour c)
        {
            keyStatus.setText (s, juce::dontSendNotification);
            keyStatus.setColour (juce::Label::textColourId, c);
        }

        void updateEnabled()
        {
            generate.setEnabled (keyOk);
            generate.setAlpha (keyOk ? 1.0f : 0.45f);
        }

        // ------------------------------------------------------------ generar
        void generateKey()
        {
            const auto name = nameEdit.getText().trim();
            const auto email = emailEdit.getText().trim();
            if (name.isEmpty() || ! email.containsChar ('@') || email.length() < 5)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Faltan datos",
                                                        juce::String::fromUTF8 ("Completá el nombre y un email válido del cliente."));
                return;
            }

            LicenseInfo info;
            info.name = name;
            info.email = email;
            info.edition = edition.getText().trim().isEmpty() ? juce::String ("Pro") : edition.getText().trim();
            info.machine = machineEdit.getText().trim().toUpperCase();
            info.issuedMs = juce::Time::currentTimeMillis();
            info.id = juce::Uuid().toString().substring (0, 12).toUpperCase();

            const auto now = juce::Time::getCurrentTime();
            auto endOfDay = [] (juce::Time t) { return juce::Time (t.getYear(), t.getMonth(), t.getDayOfMonth(), 23, 59).toMilliseconds(); };
            switch (expiry.getSelectedId())
            {
                case 2: info.expiresMs = endOfDay (now + juce::RelativeTime::days (31)); break;
                case 3: info.expiresMs = endOfDay (now + juce::RelativeTime::days (92)); break;
                case 4: info.expiresMs = endOfDay (now + juce::RelativeTime::days (183)); break;
                case 5: info.expiresMs = endOfDay (now + juce::RelativeTime::days (365)); break;
                case 6:
                {
                    const auto p = juce::StringArray::fromTokens (dateEdit.getText().trim(), "-/", "");
                    const auto y = p[0].getIntValue(), m = p[1].getIntValue(), d = p[2].getIntValue();
                    if (p.size() != 3 || y < 2024 || m < 1 || m > 12 || d < 1 || d > 31)
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, juce::String::fromUTF8 ("Fecha inválida"),
                                                                juce::String::fromUTF8 ("Escribí la fecha de vencimiento como AAAA-MM-DD (ej.: 2027-12-31)."));
                        return;
                    }
                    info.expiresMs = juce::Time (y, m - 1, d, 23, 59).toMilliseconds();
                    if (info.expiresMs <= now.toMilliseconds())
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, juce::String::fromUTF8 ("Fecha inválida"),
                                                                juce::String::fromUTF8 ("La fecha de vencimiento ya pasó."));
                        return;
                    }
                    break;
                }
                default: info.expiresMs = 0; break;
            }

            const auto key = signLicense (info, privateKey);
            // Control final: la clave tiene que pasar la misma verificación que hace el programa.
            const auto check = verifyLicense (key, productPublicKey(), info.machine.isNotEmpty() ? info.machine : machineId());
            if (! check.isValid())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Error",
                                                        juce::String::fromUTF8 ("La clave generada no pasó la verificación. No la entregues."));
                return;
            }

            lastKey = key;
            lastInfo = info;
            result.setText (key, false);
            resultInfo.setColour (juce::Label::textColourId, green);
            resultInfo.setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 ")) + juce::String::fromUTF8 ("Verificada  ·  ") + info.name + juce::String::fromUTF8 ("  ·  ")
                                    + info.edition + juce::String::fromUTF8 ("  ·  vence: ") + dateText (info.expiresMs)
                                    + juce::String::fromUTF8 ("  ·  ") + (info.machine.isEmpty() ? juce::String ("cualquier PC") : "solo PC " + info.machine)
                                    + juce::String::fromUTF8 ("  ·  ID ") + info.id,
                                juce::dontSendNotification);
            for (auto* b : { &copyKey, &copyMessage, &saveTxt })
                b->setEnabled (true);
            appendHistory (info);
        }

        juce::String customerMessage() const
        {
            juce::String s;
            s << juce::String::fromUTF8 ("¡Gracias por comprar KB - MUSIQUERO DJ!\n\n")
              << "Cliente: " << lastInfo.name << "\n"
              << juce::String::fromUTF8 ("Edición: ") << lastInfo.edition << "\n"
              << "Vencimiento: " << (lastInfo.expiresMs == 0 ? juce::String ("no vence (licencia perpetua)") : dateText (lastInfo.expiresMs)) << "\n\n"
              << "Tu clave de producto:\n\n" << lastKey << "\n\n"
              << juce::String::fromUTF8 ("Cómo activarlo:\n")
              << juce::String::fromUTF8 ("1. Instalá KB - MUSIQUERO DJ y abrilo.\n")
              << juce::String::fromUTF8 ("2. En la ventana de activación, pegá la clave completa (empieza con KBDJ1-).\n")
              << juce::String::fromUTF8 ("3. Tocá ACTIVAR. Listo: no necesita internet.\n\n")
              << juce::String::fromUTF8 ("Guardá este mensaje: si cambiás de PC o reinstalás vas a necesitar la clave.\n\n")
              << "Castro Labs\n";
            return s;
        }

        void saveForCustomer()
        {
            if (lastKey.isEmpty())
                return;
            const auto safe = juce::File::createLegalFileName (lastInfo.name).replaceCharacter (' ', '_');
            chooser = std::make_unique<juce::FileChooser> ("Guardar la licencia para el cliente",
                                                           dataFolder().getChildFile ("Licencia-KB-Musiquero-DJ-" + safe + ".txt"), "*.txt");
            dataFolder().createDirectory();
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f == juce::File())
                                          return;
                                      if (f.replaceWithText (customerMessage(), false, false, "\r\n"))
                                          flash ("Guardado: " + f.getFullPathName());
                                  });
        }

        void appendHistory (const LicenseInfo& info)
        {
            dataFolder().createDirectory();
            const auto csv = dataFolder().getChildFile ("licencias_emitidas.csv");
            juce::String line;
            if (! csv.existsAsFile())
                line << "fecha,cliente,email,id,vence,edicion,pc,clave\r\n";
            line << csvField (juce::Time (info.issuedMs).formatted ("%Y-%m-%d %H:%M")) << ","
                 << csvField (info.name) << "," << csvField (info.email) << "," << csvField (info.id) << ","
                 << csvField (dateText (info.expiresMs)) << "," << csvField (info.edition) << ","
                 << csvField (info.machine.isEmpty() ? juce::String ("cualquiera") : info.machine) << ","
                 << csvField (lastKey) << "\r\n";
            csv.appendText (line, false, false);
            historyModel.load();
            history.updateContent();
            history.repaint();
        }

        // ------------------------------------------------------------ verificar
        void verifyKey()
        {
            const auto key = verifyEdit.getText().removeCharacters (" \r\n\t");
            if (key.isEmpty())
                return;
            const auto r = verifyLicense (key, productPublicKey(), "");
            juce::String s;
            auto colour = red;
            switch (r.status)
            {
                case Status::valid:
                case Status::wrongMachine:
                    colour = green;
                    s << juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 ")) << juce::String::fromUTF8 ("Auténtica  ·  ") << r.info.name << " <" << r.info.email << juce::String::fromUTF8 (">  ·  ")
                      << r.info.edition << juce::String::fromUTF8 ("  ·  emitida ") << dateText (r.info.issuedMs) << juce::String::fromUTF8 ("  ·  vence: ") << dateText (r.info.expiresMs)
                      << juce::String::fromUTF8 ("  ·  ") << (r.info.machine.isEmpty() ? juce::String ("cualquier PC") : "solo PC " + r.info.machine);
                    break;
                case Status::expired:
                    colour = juce::Colour (0xffff8a00);
                    s << juce::String::fromUTF8 ("Auténtica pero VENCIDA el ") << dateText (r.info.expiresMs) << juce::String::fromUTF8 ("  ·  ") << r.info.name << " <" << r.info.email << ">";
                    break;
                case Status::badSignature: s = "FALSA: la firma no es de Castro Labs (o la clave fue modificada)."; break;
                case Status::missing:
                case Status::malformed:    s = juce::String::fromUTF8 ("No es una clave válida (incompleta o mal copiada)."); break;
            }
            verifyResult.setColour (juce::Label::textColourId, colour);
            verifyResult.setText (s, juce::dontSendNotification);
        }

        void flash (const juce::String& s)
        {
            resultInfo.setColour (juce::Label::textColourId, cyan);
            resultInfo.setText (s, juce::dontSendNotification);
        }

        std::unique_ptr<juce::PropertiesFile> settings;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::RSAKey privateKey;
        bool keyOk = false;
        juce::String lastKey;
        LicenseInfo lastInfo;
        juce::Array<juce::Rectangle<int>> panels;

        juce::Label keyTitle, clientTitle, resultTitle, verifyTitle, historyTitle;
        juce::Label keyStatus;
        juce::TextButton chooseKey;
        juce::Label nameLabel, emailLabel, editionLabel, expiryLabel, dateLabel, machineLabel, machineHelp;
        juce::TextEditor nameEdit, emailEdit, dateEdit, machineEdit;
        juce::ComboBox edition, expiry;
        juce::TextButton generate;
        juce::TextEditor result;
        juce::Label resultInfo;
        juce::TextButton copyKey, copyMessage, saveTxt;
        juce::TextEditor verifyEdit;
        juce::TextButton verifyButton;
        juce::Label verifyResult;
        HistoryModel historyModel;
        juce::ListBox history { "historial" };
        juce::TextButton openFolder;
    };

    class GeneratorApp final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return "KB Generador de Licencias"; }
        const juce::String getApplicationVersion() override { return KB_VERSION_STRING; }
        bool moreThanOneInstanceAllowed() override           { return true; }

        void initialise (const juce::String& commandLine) override
        {
            juce::LookAndFeel::setDefaultLookAndFeel (&look);
            const auto arg = commandLine.trim().unquoted();
            window = std::make_unique<Window> (juce::File::isAbsolutePath (arg) ? juce::File (arg) : juce::File());
        }

        void shutdown() override
        {
            window = nullptr;
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        }

    private:
        struct Window final : juce::DocumentWindow
        {
            explicit Window (const juce::File& key) : DocumentWindow ("KB Musiquero DJ - Generador de Licencias", bg, allButtons)
            {
                setUsingNativeTitleBar (true);
                setContentOwned (new GeneratorComponent (key), true);
                setResizable (true, true);
                setResizeLimits (860, 900, 2400, 2000);
                centreWithSize (getWidth(), getHeight());
                setVisible (true);
            }
            void closeButtonPressed() override { juce::JUCEApplication::quit(); }
        };

        Look look;
        std::unique_ptr<Window> window;
    };
} // namespace

START_JUCE_APPLICATION (GeneratorApp)
