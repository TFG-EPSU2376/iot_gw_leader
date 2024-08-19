graph TD
    subgraph " "
    direction LR
    D[Conectarse 
    a la red]
    D --> E{Bucle principal}
    E --> F[handleWakeUp]
    F --> Z[Calcular 
    nueva ventana 
    e hibernar]
    Z --> E
    end

