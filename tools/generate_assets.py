#!/usr/bin/env python3
import os
import subprocess
from PIL import Image, ImageDraw

def generate_assets():
    print("Verificando recursos y fuentes tipográficas de Laura...")
    os.makedirs("assets/fonts", exist_ok=True)
    os.makedirs("romfs", exist_ok=True)
    os.makedirs("data", exist_ok=True)

    # Generar icono por defecto si no existe
    icon_path = "assets/icon.png"
    if not os.path.exists(icon_path):
        img = Image.new('RGBA', (48, 48), (78, 122, 101, 255))
        d = ImageDraw.Draw(img)
        d.text((12, 12), "L", fill=(255, 255, 255, 255))
        img.save(icon_path)

    # Generar splash screen PNG y convertir a .t3x con tex3ds
    splash_png = "assets/splash.png"
    if not os.path.exists(splash_png):
        img = Image.new('RGBA', (256, 128), (250, 250, 245, 255))
        d = ImageDraw.Draw(img)
        d.text((80, 50), "Laura 3DS", fill=(78, 122, 101, 255))
        img.save(splash_png)

    splash_t3x = "romfs/splash.t3x"
    if subprocess.run(["which", "tex3ds"], capture_output=True).returncode == 0:
        subprocess.run(["tex3ds", "-i", splash_png, "-o", splash_t3x], check=True)
    else:
        # Fallback de respaldo si tex3ds no está en el entorno local (en GitHub Actions tex3ds se incluye con 3dstools)
        img.save(splash_t3x)

    print("¡Recursos listos!")

if __name__ == "__main__":
    generate_assets()
