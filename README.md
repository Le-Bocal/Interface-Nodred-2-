# ESP8266-Temperatur sensor

## Présentation du projet

Le but de ce projet est de mettre en place un système qui permet de visualiser la température de la salle du Bocal. Afin d'avoir une meilleure maîtrise sur la consommation d'énergie.

## Le choix technologique

Pour ce projet, nous partons d'une solution développée en Node-Red pour plusieurs raisons :
- Node-red est installé par défaut sur une distribution raspbian et donc on peut avoir le centre du système qui tourne sur un raspberry pi
- Le bocal n'a pas des ressources énormes à consacrer à ce développement, le système node-red parait une technologie rapide à prendre en main pour permettre aux membres du fablab de contribuer à ce développement
- il permet d'utiliser toutes les technologies qui devront permettre de mettre en place cette interface (SQlite pour la BDD, MQTT pour la communication entre modules, et une interface web simple).

## Architecture du système

![node-red architecture](/CapteurTempMQTT/content/archi-systeme.jpg)

## Fonctionnalités

Le relevé de température s'effectue toutes les heures et est sauvegardé dans une base de données. La visualisation des données se fait via un graphique. Ce même graphique peut être modifié par l'utilisateur en fonction de la période voulue.


### Objectifs

Le capteur TMP36 doit remonter ses informations en MQTT sur le raspberry pi via un module esp8266. La base de données du raspberry pi doit enregistrer les données entrantes. Enfin, les données enregistrées font suite à un relevé sur un graphique évolutif.

## Mise en place

### Montage du capteur de température TMP36 sur l'ESP8266

Le brochage du capteur TMP36 est assez simple étant un capteur analogique.

- Vcc <-> 3V3 (ou Vin(5V) selon la version du module esp)
- Vout <-> A0 (Entrée analogique du module esp)
- GND (Masse) <-> GND

![TMP36 schéma](/CapteurTempMQTT/content/schema-esp.PNG)

Remarque : le fil reliant les ports "D0" et "RST" du module ESP8266 servira pour une fonction du code wifi.

## CODE

Pour le code de l'ESP8266, les libraires suivantes seront utilisées :

* ESP8266WiFi.h / ESP8266WiFiMulti.h (pour la gestion wifi de l'esp)
* PubSubClient.h (pour gérer les messages MQTT)

### Récupération des données du caprteur de température

Pour commencer, il faut tout simplement initialiser la récupération des données du capteur TMP36.

```
    int sensorPin = 0;
 
    void setup()
    {
    Serial.begin(9600);
    }
 
    void loop()
    {
 
    int reading = analogRead(sensorPin); 
    // measure the 3v with a meter for an accurate value
    //In particular if your Arduino is USB powered
    float voltage = reading * 3; 
    voltage /= 1024.0; 
 
    // now print out the temperature
    float temperatureC = (voltage - 0.5) * 100;
    Serial.print(temperatureC); 
    Serial.println(" degrees C");
 
    delay(1000);
    }
```

### Connexion Wifi

Nous avons récupéré les données de température. Maintenant, il faut ouvrir une connexion sur le réseau, afin que pour la suite, une communication au raspberry soit possible.

```
    #include <ESP8266WiFi.h>
    #include <ESP8266WiFiMulti.h> 

    //WIFI
    const char* ssid = "TheSSID";
    const char* password = "ThePassword";

    ESP8266WiFiMulti WiFiMulti;
    WiFiClient espClient;

    void setup_wifi(){
        //connexion au wifi
        WiFiMulti.addAP(ssid, password);
        while ( WiFiMulti.run() != WL_CONNECTED ) {
            delay ( 500 );
            Serial.print ( "." );
        }
        Serial.println("");
        Serial.println("WiFi connecté");
        Serial.print("MAC : ");
        Serial.println(WiFi.macAddress());
        Serial.print("Adresse IP : ");
        Serial.println(WiFi.localIP());
    }

    void setup() {

        Serial.begin(115200);
        setup_wifi();
    }

    void loop() {
        client.loop();
    }
```

### Envoie des données en MQTT
Pour en terminer avec l'esp, nous devons faire en sorte que les données du capteur s'envoie en MQTT sur le raspberry.
Note : un broker MQTT est déjà présent sur le raspberry, donc il n'y a rien à modifier de ce côté.

```
    #include <PubSubClient.h> //Librairie pour la gestion Mqtt 

    //MQTT
    const char* mqtt_server = "192.20.2.108";//Adresse IP du Broker Mqtt
    const int mqttPort = 1883; //port use par le Broker 
    long tps=0;

    PubSubClient client(espClient);

    void setup_mqtt(){
        client.setServer(mqtt_server, mqttPort);
        client.setCallback(callback);//Déclaration de la fonction de souscription
        reconnect();
    }

    //Callback doit être présent pour souscrire a un topic et de prévoir une action 
    void callback(char* topic, byte *payload, unsigned int length) {
        Serial.println("-------Nouveau message du broker mqtt-----");
        Serial.print("Canal:");
        Serial.println(topic);
        Serial.print("donnee:");
        Serial.write(payload, length);
        Serial.println();
        if ((char)payload[0] == '1') {
            Serial.println("LED ON");
            digitalWrite(2,HIGH); 
        } 
        else {
            Serial.println("LED OFF");
            digitalWrite(2,LOW); 
        }
    }


    void reconnect(){
        while (!client.connected()) {
            Serial.println("Connection au serveur MQTT ...");
            if (client.connect("ESP32Client")) {
                Serial.println("MQTT connecté");
            }
            else {
                Serial.print("echec, code erreur= ");
                Serial.println(client.state());
                Serial.println("nouvel essai dans 2s");
                delay(2000);
            }
        }
        client.subscribe("esp/test/led");//souscription au topic led pour commander une led
    }

    //Fonction pour publier un float sur un topic 
        void mqtt_publish(String topic, float t){
        char top[topic.length()+1];
        topic.toCharArray(top,topic.length()+1);
        char t_char[50];
        String t_str = String(t);
        t_str.toCharArray(t_char, t_str.length() + 1);
        client.publish(top,t_char);
    }

    void setup() {

        Serial.begin(115200);
        setup_mqtt();
        client.publish("esp82/test", "Hello from ESP8266");
    }

    void loop() {
        reconnect();
        //On utilise pas un delay pour ne pas bloquer la réception de messages 
        if (millis()-tps>10000){
            tps=millis();
            float temp = random(30);
            mqtt_publish("esp82/temperature",temp);
            Serial.print("temperature : ");
            Serial.println(temp);
        }
    }
```

## Développement Node-RED

Maintenant, il est temps de faire le développement sur Node-RED, afin de récupérer les données, les stocker et enfin les activer.

Note : les palettes utilisées lors de ce développement sont, "node-red-node-sqlite", "node-red-dashboard".

![Flow Node-RED](/CapteurTempMQTT/content/flow-nodred.PNG)

### Récupération des données en MQTT
Pour commencer sur Node-RED, il faut s'abonner au Broker MQTT du raspberry. En faisant cela, nous recevrons automatiquement les données du capteur de température dès qu'elles sont envoyées par l'esp.

![Nœud MQTT](/CapteurTempMQTT/content/node-MQTT.PNG)

Afin de récupérer les informations du Broker, un simple nœud de sortie suffit. Pour le configurer, il faut le connecter au Broker avec la catégorie "Server" (ici en 'localhost', car le broken et Node-RED sont sur la même machine), et indiquer le topic que nous souhaitons écouter. 

### Stockage dans une base de données

![Nœud MQTT](/CapteurTempMQTT/content/node-Sqlite.PNG)

Tout d'abord, nous devons sélectionner la base de données dans laquelle les informations de température seront stockées.

Ensuite, avec des nœuds d'injection, il faut mettre en place les requêtes basiques SQL (afin de manipuler la table de données plus facilement).

Nœud pour créer la table de données :
```
    CREATE TABLE Temperature(id INTEGER PRIMARY KEY AUTOINCREMENT, temperature NUMERIC, currentdate DATE)
```

Nœud pour supprimer la table de données :
```
    DROP TABLE Temperature
```

Nœud pour visualiser les données :
```
    SELECT * FROM Temperature
```

Maintenant que les nœuds de débogage sont mis en place, nous pouvons créer le message pour insérer les informations.

Note : avant d'insérer des données, assurez-vous d'avoir créé la table de données (via le nœud d'injection)

![Nœud fonction SQL](/CapteurTempMQTT/content/node-fonction-sql.PNG)

### Visualisation des informations de température

À présent, les données sont stockées, donc nous pouvons mettre en œuvre un graphique afin de les afficher.

![Graphique de température](/CapteurTempMQTT/content/graph-temp.PNG)

Commençons par placer un nœud sqlite afin de sélectionner la dernière donnée.
Mettons la catégorie " SQL Query" avec mode "fixed Statement" et rentrer l'instruction SQL suivante :
```
    SELECT temperature, currentdate FROM Temperature ORDER BY id DESC LIMIT 1;
```

Ensuite, il faut formater le message de sortie du nœud sql avec un change-node.

![Set msg.payload](/CapteurTempMQTT/content/set-payload.PNG)

À la suite, mettons notre chart qui dessinera le graphique.

![Configuration du graph](/CapteurTempMQTT/content/node-chart.PNG)

Finalement, il faut automatiser tout ceci. Pour cela, mettons un nœud triger devant le nœud sql précédent. Ce trigger regardera la fonction "Write Query" qui crée le message pour insérer les données dans la base. Donc, chaque fois que la fonction s'activera, le trigger se déclenchera, ce qui initialisera le nœud sql de sélection de données.

![Trigger d'automatisation du système](/CapteurTempMQTT/content/node-trigger.PNG)

## Conclusion
Désormais, nous avons un système complet qui récupère des informations de température, stock les données, et les affiche sous forme de graphique. Ce qu'il va manquer, 
c'est une amélioration du système afin que l'utilisateur puisse sélectionner la période voulue.

## À suivre
Dans une deuxième partie, le capteur TMP36 devra être changé par un DHT22. Cela ajoutera une donnée pour l'hydrométrie. Il faudra la stocker et l'afficher sur un graphique séparé.
Par la suite une autre sonde thermomètre pourra être déployé dans le garage.
