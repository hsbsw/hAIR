from pathlib import Path
tools_folder = Path("tools")
mklittlefs = tools_folder / "mklittlefs"
Import("env")
print("Replace MKSPIFFSTOOL with mklittlefs")
env.Replace (MKSPIFFSTOOL = mklittlefs)
