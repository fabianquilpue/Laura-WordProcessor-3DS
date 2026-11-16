#!/usr/bin/env python3
import os
import shutil
import subprocess
from PIL import Image, ImageDraw

def generate_assets():
    print("Verificando recursos gráficos de Laura...")
    os.makedirs("assets/fonts", exist_ok=True)
    os.makedirs("romfs", exist_ok=True)
    os.makedirs("data", exist_ok=True)

    # 1. Preparar icon.png (48x48 RGBA) priorizando el archivo en la raíz
    icon_dest = "assets/icon.png"
    if os.path.exists("icon.png"):
        img = Image.open("icon.png").convert("RGBA").resize((48, 48), Image.Resampling.LANCZOS)
        img.save(icon_dest)
    elif not os.path.exists(icon_dest):
        img = Image.new("RGBA", (48, 48), (247, 245, 238, 255))
        d = ImageDraw.Draw(img)
        d.rectangle([2, 2, 45, 45], outline=(78, 122, 101, 255), width=2)
        d.text((16, 14), "L", fill=(30, 30, 30, 255))
        img.save(icon_dest)

    # 2. Preparar splash.png (96x96 RGBA) priorizando el archivo en la raíz
    splash_dest = "assets/splash.png"
    if os.path.exists("splash.png"):
        s_img = Image.open("splash.png").convert("RGBA")
        s_img.save(splash_dest)
    elif not os.path.exists(splash_dest):
        s_img = Image.new("RGBA", (96, 96), (247, 245, 238, 255))
        d = ImageDraw.Draw(s_img)
        d.rectangle([4, 4, 91, 91], outline=(78, 122, 101, 255), width=3)
        d.text((38, 36), "L|", fill=(30, 30, 30, 255))
        s_img.save(splash_dest)

    # 3. Convertir splash.png a romfs/splash.t3x con tex3ds
    splash_t3x = "romfs/splash.t3x"
    try:
        if subprocess.run(["which", "tex3ds"], capture_output=True).returncode == 0:
            subprocess.run(["tex3ds", "-o", splash_t3x, splash_dest], check=True)
        else:
            shutil.copyfile(splash_dest, splash_t3x)
    except Exception as e:
        print(f"Advertencia en tex3ds: {e}")
        shutil.copyfile(splash_dest, splash_t3x)

    print("¡Recursos listos!")

if __name__ == "__main__":
    generate_assets()
