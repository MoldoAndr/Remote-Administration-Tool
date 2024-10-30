Remote Administration Tool (RAT) - Faza Incipienta

    Instrument de administrare la distanta (RAT) dezvoltat in C 
    pentru Linux, folosind socket-uri pentru comunicare. 
    Serverul centralizat poate administra mai multi agenti (clienti) 
    instalati pe calculatoarele remote pentru colectarea datelor, 
    executarea de comenzi si monitorizarea activitatii.

Structura initiala:

    Server:
        Administreaza mai multi agenti si gestioneaza conexiunile acestora
        Trimite comenzi simultan catre agenti si primeste date 
        (informatii de sistem)

    Agenti:
        Colecteaza date de pe calculatoarele client si le trimit la server
        Executa comenzi trimise de server
        Captureaza screenshot-uri si monitorizeaza accesul la anumite site-uri

    Server: 
        Biblioteci dinamice pentru flexibilitate, performanta și actualizari mai usoare

    Client (Agent):
        Biblioteci statice pentru compatibilitate, portabilitate și distributie simplificata

Documentatie

    https://github.com/tiagorlampert/CHAOS
    https://github.com/LimerBoy/BlazeRAT
    https://github.com/wraith-labs/wraith
    https://github.com/st4inl3s5/kizagan
    https://github.com/XZB-1248/Spark

Functionalitati viitoare
    
    Executare de comenzi de la distanta
    Descarcare de fisiere
    Incarcare de fisiere
    Capturi de ecran
    Mecanisme de logging
    Key-Logger
    Alerte accesare anumite site-uri
    BlackList pentru fisiere
    
Diagrama
![Untitled Diagram](https://github.com/user-attachments/assets/6cc06881-98bb-4b4d-9f81-655ce6390ed5)
