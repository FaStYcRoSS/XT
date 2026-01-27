#This is for windows only

import subprocess
import os
import shutil
import tempfile
import time

def run_diskpart(script_content):
    """Запускает diskpart с поддержкой русской кодировки"""
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
        f.write(script_content)
        script_path = f.name
    f.close()
    
    try:
        # Указываем encoding='cp866', чтобы вывод был читаемым
        process = subprocess.run(['diskpart', '/s', script_path], 
                                 capture_output=True, text=True, encoding='cp866')
        if process.returncode != 0:
            print("[!!!] DISKPART СООБЩАЕТ:")
            print(process.stdout)
            return False
        return True
    finally:
        if os.path.exists(script_path):
            try: os.remove(script_path)
            except: pass

def create_os_image(image_name, boot_dir, home_dir, size_mb=1024):
    build_dir = os.path.abspath("build")
    if not os.path.exists(build_dir): os.makedirs(build_dir)
    
    abs_vhdx = os.path.join(build_dir, f"{image_name}.vhdx")
    abs_img = os.path.join(build_dir, f"{image_name}.img")

    print(f"[*] Подготовка образа: {abs_vhdx}")

    # --- ШАГ 0: Разблокировка файла ---
    # Пытаемся отсоединить диск, если он завис в системе
    cleanup_script = f'select vdisk file="{abs_vhdx}"\ndetach vdisk'
    run_diskpart(cleanup_script) 
    
    # Даем системе секунду "отпустить" файл
    time.sleep(1)

    # Удаляем старые файлы перед созданием
    for f in [abs_vhdx, abs_img]:
        if os.path.exists(f):
            try:
                os.remove(f)
            except PermissionError:
                print(f"[!] Не удалось удалить {f}. Файл всё еще занят процессом System.")
                print("[?] Попробуйте закрыть все окна проводника, где открыты диски Y: или Z:")
                return

    # --- ШАГ 1: Создание и разметка ---
    create_script = f"""
    create vdisk file="{abs_vhdx}" maximum={size_mb} type=fixed
    attach vdisk
    convert gpt
    create partition efi size=128
    format quick fs=fat32 label="BOOT"
    assign letter=Y
    create partition primary
    format quick fs=fat32 label="HOME"
    assign letter=Z
    """

    print("[*] Создание новой разметки...")
    if not run_diskpart(create_script):
        return

    try:
        time.sleep(2) # Ждем монтирования разделов
        
        if os.path.exists(boot_dir):
            print("[*] Копирование загрузчика...")
            shutil.copytree(boot_dir, "Y:\\", dirs_exist_ok=True)
            
        if os.path.exists(home_dir):
            print("[*] Копирование данных пользователя...")
            shutil.copytree(home_dir, "Z:\\", dirs_exist_ok=True)

    except Exception as e:
        print(f"[!] Ошибка при записи файлов: {e}")
    
    finally:
        # --- ШАГ 3: Корректное размонтирование ---
        print("[*] Освобождение ресурсов...")
        detach_script = f"""
        select volume Y
        remove letter=Y
        select volume Z
        remove letter=Z
        select vdisk file="{abs_vhdx}"
        detach vdisk
        """
        run_diskpart(detach_script)

    # --- ШАГ 4: Конвертация ---
    if os.path.exists(abs_vhdx):
        print("[*] Конвертация в RAW (.img)...")
        # Добавь полный путь к qemu-img.exe, если он не в PATH
        subprocess.run(['qemu-img', 'convert', '-f', 'vhdx', '-O', 'raw', abs_vhdx, abs_img])
        print(f"[+ DONE] Образ готов: {abs_img}")

if __name__ == "__main__":
    # Важно: Запускать от Администратора
    create_os_image("xtos", "./root", "./home")