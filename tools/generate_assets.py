
#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


def generate_assets():
    print("========================================")
    print("Preparando recursos de Laura")
    print("========================================")

    # Rutas de los recursos originales
    icon_path = "assets/icon.png"
    splash_png = "assets/splash.png"
    splash_t3x = "romfs/splash.t3x"

    # Crear directorios necesarios
    os.makedirs("assets/fonts", exist_ok=True)
    os.makedirs("romfs", exist_ok=True)
    os.makedirs("data", exist_ok=True)

    # Comprobar que existen los recursos originales
    if not os.path.isfile(icon_path):
        raise FileNotFoundError(
            "No se encuentra assets/icon.png."
        )

    if not os.path.isfile(splash_png):
        raise FileNotFoundError(
            "No se encuentra assets/splash.png."
        )

    print("[OK] Icono original encontrado.")
    print("[OK] Splash original encontrado.")

    # Localizar tex3ds
    tex3ds_path = shutil.which("tex3ds")

    if tex3ds_path is None:
        raise RuntimeError(
            "No se encuentra tex3ds. "
            "Comprueba la instalación de devkitPro."
        )

    # Convertir PNG a T3X
    print("Convirtiendo splash original a formato T3X...")

    try:
        subprocess.run(
            [
                tex3ds_path,
                "-o",
                splash_t3x,
                splash_png
            ],
            check=True
        )

    except subprocess.CalledProcessError as error:
        raise RuntimeError(
            "Falló la conversión de assets/splash.png "
            "a romfs/splash.t3x."
        ) from error

    # Verificar resultado
    if not os.path.isfile(splash_t3x):
        raise RuntimeError(
            "No se generó romfs/splash.t3x."
        )

    if os.path.getsize(splash_t3x) == 0:
        raise RuntimeError(
            "El archivo romfs/splash.t3x está vacío."
        )

    print("[OK] Splash convertido correctamente.")
    print("========================================")
    print("Recursos de Laura preparados.")
    print("========================================")


if __name__ == "__main__":
    try:
        generate_assets()

    except Exception as error:
        print()
        print("ERROR AL PREPARAR LOS RECURSOS:")
        print(error)
        sys.exit(1)
