import numpy as np

# 替换为实际的宽度和高度
width = 549  # 替换为实际宽度
height = 409  # 替换为实际高度

# 读取 RGB 文件数据
with open('r.rgb', 'rb') as f:
    rgb_data = np.fromfile(f, dtype=np.uint8)

# 将数据重新整形为 (height, width, 3)
rgb_data = rgb_data.reshape((height, width, 3))

# 将所有像素数据保存到文本文件
with open("all_pixels.txt", "w") as f:
    for row in rgb_data:
        for pixel in row:
            f.write(f"{pixel[0]}, {pixel[1]}, {pixel[2]}\n")  # 写入每个像素的 R,G,B 值

