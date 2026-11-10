# Pomodoro

Ein Pomodoro-Timer für einen Tessera-Screen: eine Fokusrunde, eine kurze Pause und nach jeder vierten Runde eine
lange Pause. Ein Ring zeigt, wie viel von der Phase übrig ist, in Bernstein beim Fokus, grün in der kurzen und blau in
der langen Pause; Punkte unter der Zeit zeigen die erledigten Runden des Zyklus.

Basiert auf [tessera-timers](https://github.com/Heronimonimo/tessera-timers) von Heronimonimo.

## Einrichtung

1. In Tessera Plugins öffnen, **Add with a link** wählen und `https://github.com/tobiii910/tessera-pomodoro` einfügen.
2. Den Screen anhaken und übernehmen. Der Screen baut einmal neu.
3. Unter Layout die Kachel **Pomodoro** aus der Gruppe Plugins der Bibliothek platzieren, in 1x1 bis 3x3.
4. In den Details des Plugins auf dem Plugins-Tab des Screens unter **Settings** die Dauern und Runden einstellen,
   falls 25, 5, 15 Minuten und 4 Runden nicht passen.

## Bedienung

- **Start**, **Pause**, **Weiter**: die angezeigte Phase.
- **Überspringen** (Pfeil): Die Phase endet sofort (eine Fokusrunde zählt), die nächste wartet auf Start.
- **Zurücksetzen** (Kreispfeil): zurück auf Fokus, Runde 1.

Bei Phasenende wartet der Timer auf ein Tippen auf **Start**, außer **Nächste Phase automatisch starten** ist an. Ein
Popup meldet das Phasenende (Einstellung **Bei Phasenende öffnen**, standardmäßig an). Screens mit Lautsprecher piepen
dreimal; der Guition 4 Zoll hat keinen und zeigt es nur an.

In der Top-Bar zeigt **Laufender Pomodoro** Phase und Minuten, solange er läuft.

## In Home Assistant

- **Pomodoro Phase**: `focus`, `short_break`, `long_break`, `paused` oder `idle`.
- **Pomodoro Runde**: die aktuelle Fokusrunde.
- **Pomodoro Phase beendet**: an vom Phasenende, bis weitergetippt wird (mit Autostart je fünf Sekunden).
