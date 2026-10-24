MIJN WEG NAAR JANNAH — INSTALLATIE EN LOKALE DATABASE

De app gebruikt geen Supabase of andere backend. Gebruikersaccounts en voortgang worden opgeslagen in de lokale browseropslag van het apparaat.

Openen:
1. Zet alle bestanden en de map `icons` op GitHub. Schakel **Settings → Pages → Deploy from a branch** in, kies de branch met de app (meestal `main`) en de map `/(root)`, en sla op. De gratis GitHub Pages-site is openbaar; publiceer er geen privégegevens of SQL-back-ups.
2. Wacht tot GitHub de site heeft gepubliceerd. Open de getoonde HTTPS-link op je telefoon; een PWA werkt niet vanaf `file://`.
3. Op Android open je de site in Chrome en kies je **Installeren** of **Toevoegen aan startscherm**. Op iPhone/iPad open je de site in Safari, tik je op **Deel** en kies je **Zet op beginscherm**.
4. De installatie maakt een startscherm-app en kan de app-shell offline openen nadat die eenmaal geladen is. Persoonlijke voortgang wordt niet met andere apparaten gesynchroniseerd.

Opslag en persoonlijke back-up:
- De app vraagt met **Bescherm lokale opslag** aan de browser om lokale gegevens niet automatisch op te ruimen. Ondersteuning verschilt per browser; dit voorkomt niet dat je gegevens verdwijnen wanneer je sitegegevens wist, de app verwijdert of van apparaat wisselt.
- Kies **Maak persoonlijke back-up** op het beginscherm om alleen het ingelogde account en diens voortgang als SQL-bestand te downloaden. Bewaar dit bestand veilig; het bevat privé-invoer en een wachtwoordhash.
- Na gegevensverlies open je de app, kies je op het inlogscherm **Kies SQL-back-up** en importeer je het bestand. Log daarna in met de accountgegevens uit de back-up. Importeren zet records terug met dezelfde account-ID; als er voor dezelfde datum al gegevens staan, worden die records overschreven.
- De keuze voor Nederlands, Engels of Turks wordt lokaal op dit apparaat bewaard. Navigatie, hoofdknoppen, checklist-/daadnamen en adhkār-titels vertalen mee; bronverwijzingen, transliteraties en een deel van de uitgebreide religieuze uitleg blijven in de oorspronkelijke tekst.

Lokale accounts:
- De eerste keer dat de app op een browser wordt geopend, maakt deze een lokaal beheerdersaccount aan.
- Eerste beheerderslogin: gebruikersnaam `Admin`, wachtwoord `Admin123`.
- Wijzig dit wachtwoord na het inloggen via **Gebruikersbeheer → Beheerderswachtwoord wijzigen**.
- Andere gebruikers maken zelf een account aan via het inlogscherm. Gebruikersnamen zijn uniek binnen de database op dit apparaat.
- Wachtwoorden worden als PBKDF2-SHA-256-hash met salt in de lokale database opgeslagen; het wachtwoord zelf wordt niet bewaard. De beheerder kan wachtwoorden niet terugzien.

Gebruikers en SQL-bestanden:
- **Gebruikersbeheer** toont uitsluitend gebruikersnamen en rollen. De beheerder kan geen routine-, deed-counter-, checklist- of dhikr-registraties van andere accounts bekijken, exporteren of importeren.
- De checklist, dagelijkse adhkār, deed counter en hartreflectie zijn samengebracht op de pagina **Schild van Imaan & daden**. Eerder opgeslagen checklistvinkjes blijven behouden; dagelijkse checklist- en dhikr-voltooiingen tellen mee in de deed counter.
- De deed counter groepeert voorbeelden per onderwerp en kan zoeken op daad, categorie of bron. De dagelijkse adhkār tonen Arabische tekst, transliteratie, voorgestelde tijden, aantallen, de genoemde deugd en verwijzingen naar Qur’an of hadith. Checklist- en dhikr-afrondingen tellen al mee; registreer dezelfde actie niet opnieuw via een handmatige deed-counter-knop.
- De dagelijkse app-rang en actuele score worden elke dag opnieuw berekend op basis van de goede daden van die dag. Daarnaast toont de app je persoonlijke topscore voor één dag; deze blijft lokaal bewaard en is opgenomen in je persoonlijke back-up. De volgende niveaus vragen steeds meer punten: de rangdrempel voor niveau *n* is `10 × n²` punten (10, 40, 90, 160, 250, 360, 490, 640, enzovoort). Elke geregistreerde goede daad, checklistafvinking of afgeronde adhkār levert 10 app-punten op; slechte registraties trekken die rang niet af. De rang is alleen een motivatiehulpmiddel, geen maatstaf voor geloof, beloning of Allah’s oordeel.
- Na inloggen onthoudt de browser op dit apparaat de account-ID en blijft de app ingelogd tot je uitlogt. Het wachtwoord of een sessietoken wordt niet opgeslagen. De sessie geldt alleen voor deze browser en app-origin; wis browsergegevens of meld je af om haar te beëindigen.
- Het beginscherm toont de datum en klok volgens de tijdzone van het apparaat, plus de islamitische datum in Arabische en Latijnse cijfers en een Ramadan-aftelling. De kalender en de komende week worden lokaal berekend met `Intl` en de Umm al-Qura-kalender. De app vraagt geen locatie op en verstuurt geen coördinaten; controleer regionale maanwaarneming, want een islamitische datum kan lokaal één dag verschillen.
- Op het beginscherm verandert een adhkār-herinnering mee met de lokale klok, met suggesties voor de vroege uren, ochtend, dag, namiddag en avond en verwijzingen naar de bronnen. Dit zijn globale tijdvakken, geen berekende gebedstijden; stem ochtend- en avondadhkār af op je lokale Fajr-, ʿAsr- en Maghrib-tijden.
- De agenda toont relevante dagen in de komende week, zoals maandag/donderdag, Jumuʿah, de witte dagen, Ramadan, Laylat al-Qadr, ʿArafah, ʿĀshūrā’ en de ʿĪds, met korte adviezen en bronverwijzingen. Dit zijn beknopte herinneringen, geen fiqh-oordeel; sommige adviezen zijn optioneel en hangen af van persoonlijke omstandigheden.
- De telknoppen bij vaste aantallen adhkār (waaronder tasbīḥ, tahlīl en istighfār) tellen elke recitatie en laten voortgang zien tot het genoemde aantal. Pas bij voltooiing telt die adhkār-kaart als één app-registratie van 10 punten; onvoltooide tussenstanden tellen niet als een afgeronde daad. Eerder afgevinkte kaarten uit oudere versies blijven als voltooid behouden.
- Deel 6 bevat afzonderlijke checklistvinkjes voor de sunnah-/vrijwillige gebeden rond Fajr, Dhuhr, ʿAsr, Maghrib en ʿIshā’. Elk vinkje telt afzonderlijk mee als 10 app-punten; dat is geen berekening van goddelijke beloning. De oude algemene sunnah-vinkjes blijven behouden.
- Bij openen van **Schild van Imaan & daden** kiest de gebruiker eerst tussen daden, voortgangslijst en adhkār. Daden en checklistgroepen staan standaard dichtgeklapt; zoeken in daden toont passende categorieën open.
- Info-knoppen op de gecombineerde pagina leggen uit hoe checklistpunten, daadregistraties, adhkār, rang en symbolische hartreflectie werken.
- Het beginscherm en Schild van Imaan tonen een groeiend of verzwakkend lichtschild rond het hart. Dit is uitsluitend een app-visualisatie: de verhouding is `goede invoer / (goede invoer + 2 × misstappen + 3 × niet-afgevinkte gebeden op eerder bijgehouden dagen)`. Niet-afgevinkt betekent niet automatisch dat een gebed is gemist. De app-gewichten zijn geen religieuze maatstaf en de percentagewaarde is geen iman-score.
- Dhikr-voltooiingen zijn per item maximaal één keer per dag af te vinken en tellen als één registratie, niet per uitgesproken woord. De genoemde deugden zijn omschrijvingen uit overleveringen; de app berekent geen exacte beloning. De overlevering over “planten van Jannah” (Jamiʿ at-Tirmidhi 3462) wordt niet omgerekend naar een vast aantal bomen.
- De **persoonlijke SQL-back-up** bevat uitsluitend het ingelogde account en diens eigen appgegevens. Import accepteert één persoonlijk accountbestand, niet een volledige database-export.
- Exporteer en importeer handmatig als je eigen gegevens tussen apparaten wilt overzetten. De app uploadt het bestand nergens.
- Bewaar en deel exportbestanden zorgvuldig: ze bevatten privégegevens en wachtwoord-hashes. Wie zo'n bestand heeft, kan de daarin opgenomen activiteiten lezen en kan proberen zwakke gebruikerswachtwoorden offline te raden.

Belangrijke beperkingen:
- GitHub Pages is statische hosting; ieder apparaat/browser heeft een eigen database. Er is geen centrale lijst van alle accounts, live synchronisatie of automatische serveropslag.
- De beheerder kan accountnamen en rollen op dit apparaat zien, maar heeft geen gebruikersinterface om persoonlijke gegevens van andere accounts te bekijken.
- De lokale admin-login is alleen een basisbescherming voor de interface. Omdat de app volledig in de browser draait, kan iemand met toegang tot het apparaat of de websitecode deze bescherming omzeilen of browsergegevens uitlezen. Gebruik dit niet voor gegevens die sterke toegangsbeveiliging vereisen.
- Browsergegevens wissen kan de lokale database verwijderen. Maak regelmatig een SQL-back-up en bewaar die op een veilige plek.
- Gegevens uit oudere versies die onder een Supabase-gebruikers-ID lokaal zijn opgeslagen, worden niet automatisch aan een nieuw lokaal account gekoppeld. Bewaar de oude browsergegevens totdat je hebt gecontroleerd of je die invoer nog nodig hebt.
- Gebruikersnamen zijn niet gegarandeerd wereldwijd uniek. Bij het importeren van twee accounts met dezelfde gebruikersnaam maar verschillende ID's moet je het conflict eerst oplossen.
- De deed-counter ×10/×1-weergave is een vereenvoudigd app-model geïnspireerd op Qur’an 6:160. De bedoelde vier tijdvakken voor dhikr zijn deels een persoonlijke routine-indeling; voor Maghrib–‘Ishā’ wordt geen afzonderlijke vastgestelde beloning geclaimd. Het lichtschild, puntentotaal en gewoontelevels meten geen geloof, spirituele staat of Allah’s oordeel. Dit is geen religieuze uitspraak of garantie op Jannah; controleer specifieke religieuze vragen bij betrouwbare geleerden.
- Voorbeelden van dhikr-bronnen in de app: Qur’an 3:17, 33:41–42 en 50:39; de drie Quls ochtend/avond (Sunan Abi Dawud 5082; Jamiʿ at-Tirmidhi 3575); Sayyid al-Istighfār (Sahih al-Bukhari 6306); tasbīḥ 100 keer (Sahih al-Bukhari 6405); tahlīl 100 keer (Sahih al-Bukhari 3293); de vier woorden als planten van Jannah (Jamiʿ at-Tirmidhi 3462); en istighfār (Sahih Muslim 2702). Hadith-nummering of beoordeling kan per uitgave verschillen.
