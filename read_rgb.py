import numpy as np


width = 549  
height = 409  

with open('r.rgb', 'rb') as f:
    rgb_data = np.fromfile(f, dtype=np.uint8)


rgb_data = rgb_data.reshape((height, width, 3))


with open("all_pixels.txt", "w") as f:
    for row in rgb_data:
        for pixel in row:
            f.write(f"{pixel[0]}, {pixel[1]}, {pixel[2]}\n")  
