MIJN WEG NAAR JANNAH — INSTALLATIE

Snel lokaal gebruiken:
1. Pak de ZIP uit.
2. Open de gepubliceerde HTTPS-site; direct openen van `index.html` als bestand schakelt Supabase-login en synchronisatie niet in.
3. De app gebruikt gratis GitHub Pages voor de website en Supabase voor accounts en versleutelde synchronisatie. Stel Supabase eerst in met de stappen hieronder.
4. Voor een app-achtige ervaring met 'Zet op beginscherm' open je de HTTPS-site op je telefoon en voeg je die toe aan het startscherm.

Gratis publiceren via GitHub Pages:
1. Maak gratis een account op https://github.com
2. Maak een nieuwe PUBLIC repository, bijvoorbeeld jannah-daily-guide.
3. Upload index.html, manifest.json, sw.js en supabase-config.js naar de hoofdmap. Volg ook de Supabase-stappen hieronder; zonder Supabase-configuratie blijft de app op het inlogscherm.
4. Ga naar Settings → Pages.
5. Kies bij Build and deployment: Deploy from a branch, kies branch main en folder /(root), en klik Save.
6. Wacht tot GitHub de site publiceert; open de URL die onder Pages verschijnt.
7. Op iPhone: open de site in Safari → Deel → Zet op beginscherm.
   Op Android: open de site in Chrome → menu ⋮ → Install app of Toevoegen aan startscherm.

Supabase-login en beheer instellen:
1. Maak een Supabase-project aan en schakel bij Authentication → Providers e-mail/wachtwoord in. Stel je site-URL en toegestane redirect-URL's in voor je GitHub Pages-domein en lokale ontwikkeladres.
2. Voer beide migraties in numerieke volgorde uit in de Supabase SQL Editor: `supabase/migrations/20261014000100_private_presence.sql` en daarna `supabase/migrations/20261014000200_encrypted_private_progress.sql`.
3. Vul de Project URL en de publieke anon/publishable key in `supabase-config.js`. Gebruik NOOIT de service-role key in dit bestand of in de browser.
4. Deploy de Edge Function met de Supabase CLI: `supabase functions deploy admin-active-users --project-ref JOUW_PROJECT_REF`. De functie gebruikt de server-side secrets `SUPABASE_URL`, `SUPABASE_ANON_KEY` en `SUPABASE_SERVICE_ROLE_KEY`; zet die in Supabase Function Secrets, nooit in de app.
5. Maak je account aan via de app en bevestig je e-mailadres. Zoek daarna de account-UUID in Supabase Authentication → Users en geef alleen jouw account beheerdersrechten met de SQL Editor: `insert into public.app_admins (user_id) select id from auth.users where email = 'JOUW_ADMIN_EMAIL' on conflict do nothing;`
6. Publiceer de app via HTTPS. Test eerst dat een gewoon account de adminlijst niet kan openen en dat een admin uitsluitend e-mailadressen en aanwezigheidsstatus ziet.

Account en privacy:
- Inloggen vereist het ingestelde Supabase-project. Supabase verwerkt accountgegevens voor authenticatie en de aanwezigheidsfunctie bewaart account-ID's en laatste activiteitstijd.
- De beheerfunctie toont alleen e-mail en online/laatst-actief status (online bij een heartbeat in de laatste 90 seconden; recent actief tot 5 minuten). De functie geeft geen checklist, deed-counter, hartreflectie of routinegegevens terug. Accounts kunnen via RLS alleen hun eigen aanwezigheidsrecord lezen of bijwerken; alleen de serverfunctie kan alle aanwezigheid controleren.
- Checklist, eigen daden en hartreflecties worden lokaal bijgehouden en automatisch naar een privé Supabase-tabel gesynchroniseerd. De app versleutelt de volledige voortgang eerst op het apparaat met AES-256-GCM; een aparte privacy-wachtzin (minimaal 12 tekens) wordt met PBKDF2 afgeleid en wordt nooit naar Supabase verstuurd of opgeslagen. De database bewaart het account-ID en de ciphertext, willekeurige salt, IV, revisie en wijzigingstijd. Rijbeveiliging beperkt elk account tot de eigen versleutelde rij. Eigen daadtegels, routine, daden en reflecties zijn inhoudelijk versleuteld; de beheerfunctie kan deze niet ontsleutelen.
- Gebruik dezelfde privacy-wachtzin op ieder apparaat. De privacy-wachtzin is onafhankelijk van je accountwachtwoord: wachtwoordherstel herstelt de privacy-wachtzin niet. Als je de privacy-wachtzin vergeet, kan de cloudkopie niet meer worden ontsleuteld. Bewaar hem dus veilig; er bestaat geen achterdeur of wachtwoordreset voor de versleutelde gegevens.
- Bestaande gegevens van vóór de accountlogin kunnen op het oorspronkelijke apparaat eenmalig worden geïmporteerd en daarna versleuteld worden gesynchroniseerd.
- Lokale browseropslag blijft beschikbaar als lokale kopie, maar is niet versleuteld tegen iemand die toegang heeft tot jouw ontgrendelde browserprofiel of apparaat. De cloudkopie is end-to-end versleuteld en blijft toegankelijk op een nieuw apparaat na inloggen en invoeren van dezelfde privacy-wachtzin.
- De synchronisatie gebruikt revisies en een conflictcontrole zodat gelijktijdige wijzigingen niet stilzwijgend elkaars cloudkopie overschrijven. Als de verbinding uitvalt, blijven wijzigingen lokaal staan en probeert de app later opnieuw te synchroniseren.
- GitHub Pages en Supabase bieden gratis abonnementen, maar geen onbeperkte of gegarandeerde levenslange opslag: providerlimieten kunnen veranderen en gratis projecten kunnen worden gepauzeerd. De app schrijft geen betaalde functies of upgrades voor; controleer zelf de actuele quota van de providers.
- De opgeslagen ciphertext is voor iemand met databasebeheer zichtbaar, net als omvang en wijzigingstijd, maar zonder privacy-wachtzin niet leesbaar. Dit beschermt niet tegen kwaadaardige wijzigingen in de gepubliceerde websitecode: wie die code beheert, kan in theorie een latere appversie aanpassen om ingevoerde wachtzinnen te onderscheppen. Host daarom alleen een appbuild die je vertrouwt.

Let op:
- De accountlijst is privacygevoelige aanwezigheidsinformatie; geef beheerdersrechten alleen aan vertrouwde accounts en deel geen service-role key.
- De deed counter gebruikt Qur’an 6:160 alleen als eenvoudige basisweergave (goede daad ×10, slechte daad ×1); routine-items tellen als positieve invoer mee. Het daghart en cumulatieve hart op het startscherm zijn symbolische reflecties geïnspireerd door Qur’an 83:14 en Jamiʿ at-Tirmidhi 3334. Er is geen numerieke religieuze grens voor een volledig zwart hart in de app ingesteld. Animaties en totalen tonen app-invoer, niet iemands werkelijke hart, iman, aanvaarde tawbah of religieuze staat; er is geen één-op-één maat voor daden verzonnen. Tawbah en herstel kunnen per dag als reflectie worden gemarkeerd, zonder claim dat de app vergiffenis kan vaststellen.
- Gewoontelevels tellen unieke dagen met aangevinkte routine-items of een vastgelegde tawbah-reflectie. Ze meten gewoonte-opbouw, niet geloof, spirituele rang of de uitkomst van daden. De markering “ernstige zonde” verwijst naar de zeven vernietigende zonden in Sahih al-Bukhari 2766 en Sahih Muslim 89; er is geen extra numerieke factor bedacht.
- Het hart op het startscherm vat de geregistreerde goede stappen en misstappen van alle opgeslagen dagen samen. Het wordt niet volledig zwart: de religieuze bronnen geven geen numerieke app-drempel en de visualisatie mag geen oordeel suggereren.
- Eigen daden kunnen per categorie met een optionele bronvermelding worden toegevoegd. De totalen zijn na synchronisatie beschikbaar op je andere apparaten.
- Dit is een persoonlijke tracker, geen religieuze uitspraak of garantie op Jannah. Vraag een betrouwbare geleerde voor specifieke fiqh-vragen.
