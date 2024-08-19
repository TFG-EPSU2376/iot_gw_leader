sequenceDiagram
participant LM as Edge (WiFi.LoRa)
participant LT as Leader (LoRa,Thread)
participant CT as Child (Thread)

%% Conexión inicial
LT->>+LM: Aviso de conexión
LM-->>-LT: Informa de hora de la red
Note right of LT: Modo escucha

%% Conexión del Child Thread
CT->>+LT: Aviso de conexión
LT-->>-CT: Informa parámetros red
Note right of CT: Modo monitorización

%% Envío de datos
CT->>+LT: Envía datos sensor
LT->>+LM: Envía datos sensor
LM-->>LM: Transmitir a la plataforma (MQTT)
deactivate LT
deactivate LM
