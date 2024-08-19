graph LR
    subgraph " "
    direction LR
    H[Con leader Thread] --> I{Es el leader?}
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