## server.c
Acest fișier implementează funcționalitatea serverului. Serverul gestionează conexiunile clienților și comunicarea între aceștia. Este responsabil pentru:
- Acceptarea conexiunilor de la clienți.
- Gestionarea mesajelor primite și transmise.
- Menținerea unei liste de abonați și a stării acestora.

## subscriber.c
Acest fișier implementează funcționalitatea unui client abonat. Clientul poate:
- Să se conecteze la server.
- Să primească mesaje de la server.
- Să trimită comenzi către server pentru a se abona sau dezabona de la anumite topicuri.

### server.c
Serverul este implementat pentru a gestiona mai mulți clienți simultan folosind multiplexarea I/O (de exemplu, cu `select()`). Funcționalitățile principale includ:
- **Acceptarea conexiunilor noi**: Serverul ascultă pe un port specific și acceptă conexiuni de la clienți noi.
- **Gestionarea mesajelor**: Mesajele primite de la clienți sunt procesate și, în funcție de tipul lor, sunt fie retransmise altor clienți, fie utilizate pentru actualizarea stării interne.
- **Lista de abonați**: Serverul menține o listă de clienți abonați la diverse topicuri. Această listă este utilizată pentru a trimite mesaje doar clienților interesați.

### subscriber.c
Clientul abonat este implementat pentru a interacționa cu serverul și include următoarele funcționalități:
- **Conectarea la server**: Clientul se conectează la server utilizând adresa IP și portul specificat.
- **Abonarea la topicuri**: Clientul poate trimite comenzi către server pentru a se abona la anumite topicuri de interes.
- **Dezabonarea de la topicuri**: Similar, clientul poate solicita dezabonarea de la topicuri.
- **Recepționarea mesajelor**: Mesajele trimise de server sunt afișate în timp real, în funcție de topicurile la care clientul este abonat.

### Protocol de comunicare
Serverul și clienții comunică utilizând un protocol simplu bazat pe mesaje text. Fiecare mesaj are un format specific, de exemplu:
- `subscribe <topic>`: Comandă trimisă de client pentru a se abona la un topic.
- `unsubscribe <topic>`: Comandă trimisă de client pentru a se dezabona.
- Mesajele de la server conțin informații despre topic și conținutul mesajului.

### Erori și gestionare
- Serverul gestionează deconectările neașteptate ale clienților și curăță resursele asociate acestora.
- Clienții verifică dacă conexiunea cu serverul este activă și afișează mesaje de eroare în caz de probleme.

### Extensii posibile
- Adăugarea autentificării pentru clienți.
- Implementarea unui mecanism de stocare a mesajelor pentru clienții deconectați.
- Suport pentru criptarea comunicației între server și clienți.
