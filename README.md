# Trabajo Final de C - Simulación de Artemis (Escenario 4)



## a) Integrantes y Comisión ##

| Integrante | Comisión |
| :--- | :--- |
| **Borges, Facundo** | K 1.1 |
| **Contreras, Santiago** | K 1.1 |
| **Dabski, Santiago** | K 1.1 |
| **Retamozo, Kevin** | K 1.1 |

Aclaración: La mayoria de commits estan hechos desde la cuenta facunzilla-png ya que el equipo se reunio en la casa de este usuario para realizar el trabajo; Borges, Facundo lo revisaba y lo subia.
Aclaración 2: Había una versión de consola pero fue removida.
Aclaración 3: Borges, Facundo es humano y puede ser que se haya olvidado subir algunos archivos y haya tenido que subirlos juntos en el mismo commit evidenciado en algunos commits.

---



## b) Descripción del Escenario y Reglas del Juego



### Escenario Asignado: Situación-Problema 4 — Reingreso Atmosférico, Frenado Aerodinámico y Amerizaje (Trabajo, Energía e Impacto)



Tras completar el sobrevuelo lunar de la misión Artemis, el módulo de tripulación Orion ($m = 10.400,0\text{ kg}$) se separa del módulo de servicio y reingresa a la atmósfera terrestre a una velocidad hiperveloz de $v_i = 11.000,0\text{ m/s}$ ($11,0\text{ km/s}$, Mach 32) desde una altitud de $h_i = 120.000,0\text{ m}$ ($120,0\text{ km}$). 



Durante el descenso, el escudo térmico disipa más del $99,9\%$ de la energía mecánica inicial mediante el trabajo no conservativo de la fuerza de resistencia aerodinámica del aire ($W_{\text{roz}} = \Delta E_m$), reduciendo la rapidez de la cápsula a lo largo de una fase crítica de $\Delta t = 400,0\text{ s}$ hasta alcanzar $v_f = 25,0\text{ m/s}$ a una altitud de $h_f = 2.500,0\text{ m}$, donde se despliegan los paracaídas principales. Finalmente, la cápsula impacta en el Océano Pacífico (amerizaje) a una velocidad nominal de $v_{\text{impacto}} = 8,0\text{ m/s}$, hundiéndose $d = 1,50\text{ m}$ hasta detenerse momentáneamente bajo la fuerza de frenado del agua:



$$F = m \cdot \left(g + \frac{v^2}{2d}\right)$$



---



### Reglas del Juego



El usuario asume el rol de Director de Vuelo de la cápsula Orion. Antes de comenzar, se elige un nivel de dificultad (**Fácil**, **Normal** o **Difícil**), el cual define la cantidad de combustible RCS disponible, la intensidad de las perturbaciones atmosféricas y los límites máximos de fuerzas $g$ e impacto estructural que soportan la tripulación y el casco.



La misión se divide en **3 eventos interactivos por turnos** que deben superarse en orden:



1. **Evento 1 — Alineación de Reingreso con Propulsores RCS (3 a 4 turnos):**

   * **Situación inicial:** La nave se aproxima al borde atmosférico ($120\text{ km}$) con un ángulo $\gamma$ desviado al azar y una velocidad de giro inicial ($\omega$).

   * **Mecánica:** Aplicando la 1.ª Ley de Newton (inercia rotacional), la cápsula conserva su velocidad angular $\omega$ entre turnos ($\gamma_{\text{nuevo}} = \gamma + \omega$). Antes de cada turno, el Radar informa la anomalía gravitatoria/atmosférica que afectará el giro. El jugador debe administrar su combustible RCS eligiendo entre:

     * *Pulso Suave Prógrado / Retrógrado* ($\pm 0,4^\circ/\text{turno}$, consume $1\text{ u.}$ de RCS).

     * *Pulso Fuerte Prógrado / Retrógrado* ($\pm 1,0^\circ/\text{turno}$, consume $2\text{ u.}$ de RCS y recalienta las toberas bloqueando el pulso fuerte por 1 turno).

     * *Mantener Actitud* ($0^\circ/\text{turno}$, consume $0\text{ RCS}$ y enfría las toberas).

   * **Condición de victoria:** Llegar a la interfaz atmosférica ($t = 0$) dentro de la ventana segura ($5,5^\circ \le \gamma \le 7,0^\circ$) y con el giro estabilizado ($|\omega| \le 0,50^\circ/\text{turno}$). Si $\gamma < 5,5^\circ$ se produce un rebote atmosférico hacia el espacio; si $\gamma > 7,0^\circ$ la desaceleración y el calentamiento resultan destructivos.



2. **Evento 2 — Gestión de Sustentación en el Frenado Aerodinámico (4 fases de $100\text{ s}$):**

   * **Mecánica:** Durante los $400\text{ s}$ de máximo frenado, el radar detecta variaciones de densidad en cada capa atmosférica. En cada una de las 4 fases, el jugador rota la cápsula sobre su eje (*bank angle*) para regular la sustentación:

     * *Sustentación Alta (subir a aire tenue):* Frena menos ($0,78\times$), reduce las fuerzas $g$ del tramo y enfría el escudo térmico ($-15\%$).

     * *Perfil Nominal (equilibrado):* Frenado estándar ($1,00\times$) y calentamiento moderado ($+18\%$).

     * *Inmersión Densa (bajar a aire denso):* Maximiza el trabajo de rozamiento ($1,25\times$), pero incrementa el pico de fuerzas $g$ y eleva fuertemente la temperatura del escudo ($+38\%$).

   * **Condición de victoria:** Disipar la energía cinética suficiente para finalizar la Fase 4 con una velocidad segura de apertura de paracaídas ($\le 85\text{ m/s}$), sin que ningún pico de desaceleración supere el límite de fuerzas $g$ de la dificultad ni la temperatura del escudo supere el $100\%$.



3. **Evento 3 — Secuencia de Paracaídas y Amerizaje (2 decisiones críticas):**

   * **Decisión 1 (Altitud de despliegue de paracaídas):** Determina la velocidad final de impacto $v$ según el estado meteorológico. *Apertura Temprana* reduce la velocidad de caída ($0,92\times$), pero si hay ráfagas intensas ($> 108\%$) el tirón desgarra las cuerdas; *Apertura Nominal* mantiene los parámetros de diseño ($2.500\text{ m}$); *Apertura Tardía* evita roturas por viento pero impacta un $45\%$ más rápido ($1,45\times$).

   * **Decisión 2 (Actitud de impacto en el agua):** Determina la profundidad de hundimiento $d$ en la ecuación de impacto. *Entrada en Cuña ($28^\circ$)* aumenta el hundimiento ($1,20\times$, $d = 1,80\text{ m}$) amortiguando el golpe (salvo oleaje extremo); *Entrada Nominal ($18^\circ$)* logra el hundimiento estándar ($1,50\text{ m}$); *Entrada Plana ($0^\circ$)* frena en seco en la superficie ($0,60\times$, $d = 0,90\text{ m}$) multiplicando la fuerza sobre el casco.

   * **Condición de victoria:** Que la fuerza promedio ejercida por el agua (expresada en múltiplos del peso de la nave) no supere el límite estructural de la dificultad elegida.



### Sistema de Puntaje

* **Puntos por evento:** $+300\text{ puntos}$ por cada evento superado (hasta $900\text{ puntos}$ base).

* **Penalización por consumo:** $-25\text{ puntos}$ por cada unidad de combustible RCS utilizada en el Evento 1.

* **Bonus de precisión:** $+100\text{ puntos}$ adicionales si se completa toda la misión con éxito habiendo ingresado a la atmósfera con un ángulo $\gamma$ a $\pm 0,25^\circ$ del centro ideal del corredor ($6,25^\circ$).

* **Puntaje máximo posible:** $1.000\text{ puntos}$.



---



## c) Breve Detalle de Funcionamiento y Arquitectura



El sistema fue diseñado en lenguaje C (estándar C99) siguiendo una arquitectura modular estricta que separa el motor de física y reglas de la capa de renderizado gráfico:



* **Arquitectura de Módulos (`.c` y `.h`):**

  * `artemis.h` y `artemis.c`: Contienen las constantes físicas del escenario, las estructuras de datos principales (`DatosMision`, `Dificultad`, `TurnoRCS`, `PasoFrenado` y `Reporte`) y las funciones puras del motor físico y lógico (`energia_mecanica`, `calcular_trabajo_rozamiento`, `calcular_desaceleracion_media`, `calcular_fuerza_amerizaje`, `aplicar_turno_rcs`, `aplicar_turno_frenado`, `elegir_apertura_paracaidas`, `elegir_actitud_impacto` y `calcular_puntaje`), desacopladas de toda entrada/salida directa.

  * `validaciones.h` y `validaciones.c`: Módulo de validación de datos encargado de verificar rangos numéricos mediante `strtod()` (aceptando punto o coma decimal indistintamente) y realizar la validación física cruzada (`datos_coherentes`) para asegurar que los parámetros personalizados ingresados por el usuario tengan coherencia entre etapas.

  * `main.c`, `interfaz.h` e `interfaz.c`: Implementan la aplicación gráfica completa mediante una máquina de estados finitos (`struct Juego` y `enum Pantalla`) que corre a 60 FPS sobre `raylib` + `raygui`. Incluye ilustraciones vectoriales animadas, un laboratorio interactivo de análisis físico y las pantallas interactivas de los tres minijuegos.



---



## d) Requisitos, Instalación de Bibliotecas y Compilación



El simulador está desarrollado en lenguaje **C (estándar C99)** y utiliza una interfaz gráfica interactiva construida con **raylib** y **raygui**.



### 1. Bibliotecas Necesarias



* **`raylib`:** Biblioteca gráfica de bajo nivel utilizada para la creación de la ventana, el bucle de renderizado a 60 FPS y el dibujo vectorial en tiempo real (cápsula Orion, curvas de trayectoria y velocidad, atmósfera y paracaídas).

* **`raygui`:** Biblioteca de interfaz de usuario de modo inmediato (*immediate-mode GUI*) de un solo archivo (`raygui.h`), utilizada para los botones interactivos, cuadros de entrada de texto y controles deslizantes (*sliders*). *(Ya incluida dentro del directorio del proyecto).*

* **Bibliotecas estándar y del sistema:** Matemática (`-lm`) y, en sistemas Windows, las APIs nativas de gráficos y multimedia (`-lopengl32`, `-lgdi32`, `-lwinmm`).



---



### 2. Cómo Instalar las Dependencias



#### En Windows (mediante MSYS2 — Entorno UCRT64)

1. Descargar e instalar **MSYS2** desde [msys2.org](https://www.msys2.org/).

2. Abrir la terminal **MSYS2 UCRT64** y actualizar la base de paquetes ejecutando:

   ```bash

   pacman -Syu

   ```

3. Instalar el compilador **GCC**, la herramienta **Make** y la biblioteca **raylib** con el siguiente comando:

   ```bash

   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-raylib

   ```

4. Verificar que `raygui.h` se encuentre en la carpeta raíz del proyecto (o instalarlo opcionalmente vía `pacman -S mingw-w64-ucrt-x86_64-raygui`).



#### En Linux (Ubuntu / Debian y derivados)

1. Abrir una terminal e instalar las herramientas de compilación y las dependencias gráficas de `raylib`:

   ```bash

   sudo apt update

   sudo apt install build-essential git cmake libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev

   ```

2. Clonar y compilar `raylib` de forma global:

   ```bash

   git clone --depth 1 https://github.com/raysan5/raylib.git

   cd raylib/src

   make PLATFORM=PLATFORM_DESKTOP

   sudo make install

   ```



---



### 3. Compilación y Ejecución



#### Opción A: Usando `Makefile` (Recomendado)

Ubicarse en la carpeta raíz del proyecto desde la terminal y ejecutar:



```bash

make

```



Para limpiar los archivos objeto (`.o`) y el ejecutable generado anteriormente:

```bash

make clean

```



#### Opción B: Compilación manual con `gcc`

* **En Windows (MSYS2 UCRT64):**

  ```bash

  gcc -std=c99 -Wall -Wextra -I. main.c interfaz.c artemis.c validaciones.c -o artemis.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lm

  ```

* **En Linux:**

  ```bash

  gcc -std=c99 -Wall -Wextra -I. main.c interfaz.c artemis.c validaciones.c -o artemis -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

  ```



#### Ejecución del Programa

Una vez compilado, iniciar el simulador ejecutando:

* **Windows:**

  ```bash

  ./artemis.exe

  ```

* **Linux:**

  ```bash

  ./artemis

  ```



---

## e) Registro detallado de inteligencia artificial

En un principio, se le mando el documento con la consigna a Claude, utilizando el prompt: "Buenas, necesito que me plantees como hacer este trabajo de forma ordenada y tranquila, mas que nada para darnos una idea de como tendríamos que trabajar con mi grupo. Mi grupo tiene que hacer el escenario 4. Y si podes hacer un modelo de código de como quedaría para utilizarlo de plantilla sobre la que hacer nuestros cambios según sea necesario". 

También se utilizo Gemini 4 Argon para aprender a utilizar la librería Raylib y dar ideas de como se podrían hacer minijuegos dinamicos.
