import mss
try:
    with mss.mss() as sct:
        print(sct.monitors)
except Exception as e:
    print(f"Error: {e}")
