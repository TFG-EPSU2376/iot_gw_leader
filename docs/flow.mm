graph TD
    subgraph "Main"
    B[Iniciar dispositivo]
    B --> C[Enviar mensaje LoRa de conexión]
    C --> D[Recibir mensaje con tiempo de la red]
    D --> E{Bucle principal}
    E --> F[handleWakeUp]
    F --> G[Calcular nueva ventana wakeUp]
    G --> Z[Hibernar hasta nueva ventana]
    Z --> E
    end

    subgraph "handleWakeUp"
    H[Inicializar leader Thread] --> I{Es el leader?}
    I -->|No| I
    I -->|Sí| J[Modo Escucha]
    J --> K{Nuevo mensaje}
    K -->|Conexión| L[Enviar mensaje Thread \ncon parámetros de red]
    K -->|Data| M[Transmitir mensaje \npor LoRa al núcleo]
     J --> N[Timer]
    N --> O[Desactivar Leader Thread]
    O --> P{Está desactivado?}
    P -->|No| P
    P -->|Sí| Q[Fin]
    end


 