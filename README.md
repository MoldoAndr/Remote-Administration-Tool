Remote Administration Tool (RAT) - Faza Incipienta

Instrument de administrare la distanta (RAT) dezvoltat in C pentru Linux, folosind socket-uri pentru comunicare. 
Serverul centralizat poate administra mai multi agenti (clienti) instalati pe calculatoarele remote pentru colectarea datelor, 
executarea de comenzi si monitorizarea activitatii.

Structura initiala:

    Server:
        Administreaza mai multi agenti si gestioneaza conexiunile acestora
        Trimite comenzi simultan catre agenti si primeste date (ex: screenshot-uri, informatii de sistem)

    Agenti:
        Colecteaza date de pe calculatoarele client si le trimit la server
        Executa comenzi trimise de server
        Captureaza screenshot-uri si monitorizeaza accesul la anumite site-uri

    Server: Biblioteci dinamice pentru flexibilitate, performanta și actualizari mai usoare
    Client (Agent): Biblioteci statice pentru compatibilitate, portabilitate și distributie simplificata

Structura Proiectului

    Server (Centralizator): Gestioneaza conexiunile cu agentii si trimite comenzi catre acestia
    Agenti (Clienti): Se conecteaza la server si asteapta comenzi pentru a colecta date sau a executa actiuni pe statii
    
<img width="1325" alt="Structura_Initiala" src="https://github.com/user-attachments/assets/c5541d8c-1365-4131-a98d-1f21fd12baaf">
