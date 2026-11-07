## About

Attica is the KDE Frameworks client library for the Open Collaboration Services API, the REST
interface the KDE Store (`store.kde.org`) speaks. It lists providers, categories and content, and
fetches download links for them; [knewstuff](#port-knewstuff) is built on it, and the "Get New…"
dialogs of KDE programs reach the store through the two together. It needs nothing beyond Qt Core
and Qt Network.
