import os

def main():
    print("Verificando recursos y fuentes tipográficas de Laura...")
    os.makedirs("assets/fonts", exist_ok=True)
    os.makedirs("data", exist_ok=True)
    print("¡Recursos listos!")

if __name__ == "__main__":
    main()
