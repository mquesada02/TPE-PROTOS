# Trabajo Práctico Final - Protocolos de Comunicación
Nicolás Casella - Catalina Müller - Manuel Quesada - Timoteo Smart (GRUPO 3)

## Compilación y ejecución

- __Es necesario disponer de un entorno linux para poder correr los programas__

### Tracker

Para iniciar el servidor tracker, posicionarse en la carpeta ```/tracker``` y correr el comando:

```
make clean all
```

Esto creará un ejecutable, para obtener una lista de argumentos posibles agregar el flag ```-h```
```
./tracker -<flag1> <argumento> -<flag2> <argumento>
```

### Client

Para iniciar la aplicación cliente, posicionarse en la carpeta ```/client``` y correr el comando:

```
make clean all
```

Esto creará un ejecutable, para obtener una lista de argumentos posibles agregar el flag ```-h```
```
./client -<flag1> <argumento> -<flag2> <argumento>
```


## Uso del cliente

Cualquier archivo en la carpeta ```/repository``` será añadido al tracker y podrá ser compartido con otros clientes.

En caso de descargar un archivo, este será instalado en la misma carpeta.

Una lista de comandos posibles puede ser obtenido al ejecutar ```help``` dentro de la línea de comandos de la aplicación.
