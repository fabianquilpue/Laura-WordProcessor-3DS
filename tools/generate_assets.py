#!/usr/bin/env python3
import os
import shutil
import subprocess
from PIL import Image, ImageDraw

def generate_assets():
    print("Verificando recursos y fuentes tipográficas de Laura...")
    os.makedirs("assets/fonts", exist_ok=True)
    os.makedirs("romfs", exist_ok=True)
    os.makedirs("data", exist_ok=True)

    icon_path = "assets/icon.png"
    # Si el usuario colocó un icon.png en la raíz, úsalo prioritariamente
    if os.path.exists("icon.png"):
        shutil.copy("icon.png", icon_path)
    
    if not os.path.exists(icon_path):
        img = Image.new('RGBA', (48, 48), (247, 245, 238, 255))
        d = ImageDraw.Draw(img)
        d.text((16, 12), "L", fill=(30, 30, 30, 255))
        img.save(icon_path)
    else:
        # Asegurar dimensiones exactas de 48x48 RGBA para smdhtool
        img = Image.open(icon_path).convert('RGBA').resize((48, 48), Image.Resampling.LANCZOS)
        img.save(icon_path)

    splash_png = "assets/splash.png"
    img = Image.new('RGBA', (256, 128), (250, 250, 245, 255))
    d = ImageDraw.Draw(img)
    d.text((80, 50), "Laura 3DS", fill=(78, 122, 101, 255))
    img.save(splash_png)

    splash_t3x = "romfs/splash.t3x"
    try:
        if subprocess.run(["which", "tex3ds"], capture_output=True).returncode == 0:
            subprocess.run(["tex3ds", "-o", splash_t3x, splash_png], check=True)
        else:
            img.save(splash_t3x)
    except Exception as e:
        print(f"Advertencia al ejecutar tex3ds: {e}. Usando respaldo.")
        img.save(splash_t3x)

    print("¡Recursos listos!")

if __name__ == "__main__":
    generate_assets()
