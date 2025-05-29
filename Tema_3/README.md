***Tema 3 - Client HTTP***

In tema am folosit scheletul lab-ului 9(de acolo si comentariile din requests.c si helpers.c)

**Implementare:**
- Pentru fiecare comanda deschid o noua conexiune catre sever
        (asta pentru a face clientul sa se poata conecta la orice server, nu doar la cel din enunt);
- Foloses un `cookies_exist` si `jwt_token_exists` pentru a stii daca sa adaug cookies/ token
in cerere;

**Utilizare:**
- Se ruleaza `make` urmat `./client` sau `make run`;

**Dificultati:**
- Enunt vag la `add_collection`, a durat ceva pana am inteles cum functioneaza;
- Probleme cu checker-ul, nu parseaza bine anumite comenzi;
- La `get_movie`, server-ul imi trimtie `rating` sub forma de `string` in loc de
        `double`, a durat ceva pana sa ma prind;
- Termenul de expirare al `JWT Token` era destul de mic, in PostMan trebuia sa fac
        la fiecare 5 min o noua cerere catre `get_access`;

**Pentru echipa:**


Super misto tema, usor de inteles, dar nu mai puneti serverul din nou pe un ***<ins>raspberry pi</ins>***