Remote Administration Tool (RAT) - Faza Incipienta

Acest proiect este un instrument de administrare la distanta (RAT) dezvoltat in C pentru Linux, folosind socket-uri pentru comunicare. Serverul centralizat poate administra mai multi agenti (clienti) instalati pe calculatoarele remote pentru colectarea datelor, executarea de comenzi si monitorizarea activitatii.
Functionalitati

In aceasta versiune incipienta, RAT-ul ofera urmatoarele:

    Server:
        Administreaza mai multi agenti si gestioneaza conexiunile acestora.
        Trimite comenzi simultan catre agenti si primeste date (ex: screenshot-uri, informatii de sistem).

    Agenti:
        Colecteaza date de pe calculatoarele client si le trimit la server.
        Executa comenzi trimise de server.
        Captureaza screenshot-uri si monitorizeaza accesul la anumite site-uri.

Structura Proiectului

    Server (Centralizator): Gestioneaza conexiunile cu agentii si trimite comenzi catre acestia.
    Agenti (Clienti): Se conecteaza la server si asteapta comenzi pentru a colecta date sau a executa actiuni pe calculatoarele lor.

Cerinte

    Sistem de operare: Linux
    Limbaj: C
    Biblioteci: POSIX sockets, pthread (pentru multi-threading)
