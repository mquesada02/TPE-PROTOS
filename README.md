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

## Tests

A continuación se describen las siguientes secuencias de comandos para testear las aplicaciones, se asume que tanto client como tracker fueron compilados:
```
REGISTRAR UN USUARIO
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. dentro de la consola de comandos de client, correr:
    - register testuser testpassword
    - files
    - logout
```
```
INGRESAR SESIÓN
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
    - files
    - logout
```
```
DESCARGAR UN ARCHIVO LOCAL
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
    - files
    - download <hash>
    - logout
```
```
DESCARGAR UN ARCHIVO DESDE OTRO CLIENTE
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
    - files
    - download <hash>
    - logout
```
```
DESCARGAR UN ARCHIVO DESDE OTRO CLIENTE
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
5. dentro de la segunda consola, correr:
    - register testuser2 testpassword2
    - files
    - download <hash>
    -logout
```
```
VER LOS SEEDERS CONECTADOS
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
5. dentro de la segunda consola, correr:
    - register testuser2 testpassword2
    - files
    - download <hash>
    - seeders
```
```
VER LOS LEECHERS CONECTADOS
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
5. dentro de la segunda consola, correr:
    - register testuser2 testpassword2
    - files
    - download <hash>
6. volver a la primer consola, correr:
    - leechers
```
```
VER EL ESTADO DE LA DESCARGA ACTUAL
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
5. dentro de la segunda consola, correr:
    - register testuser2 testpassword2
    - files
    - download <hash>
    - dstatus
```
```
PAUSAR, REANUDAR Y CANCELAR DESCARGA
1. iniciar tracker con ./tracker
2. iniciar client con ./client
3. iniciar una segunda sesión con ./client -L 2526, puede ser cualquier otro puerto, mientras que no sea el default
4. dentro de la consola de comandos de client, correr:
    - login testuser testpassword
5. dentro de la segunda consola, correr:
    - register testuser2 testpassword2
    - files
    - download <hash>
    - pause
    - dstatus
    - resume
    - cancel
    - logout
```
