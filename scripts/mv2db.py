import math

def db_to_mvp(dB):
    """Convert dB to mV/Pa"""
    mvp = 10 ** (dB / 20) * 1000
    return round(mvp, 4)

def mvp_to_db(mvp):
    """Convert mV/Pa to dB"""
    dB = 20 * math.log10(mvp / 1000)
    return round(dB, 2)

# Example usage:
print("Convert -26 dB to mV/Pa:", db_to_mvp(-26), "mV/Pa")
print("Convert 1.8 mV/Pa to dB:", mvp_to_db(1.8), "dB")
