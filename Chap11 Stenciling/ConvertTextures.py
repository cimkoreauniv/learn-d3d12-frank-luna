'''
Converts bmp image to png with alpha component.

교재에서 제공하는 번개 이미지는 bmp 형태로 제공되기 때문에
알파 채널을 (R+G+B)//3으로 설정하여 배경이 투명해지도록 한다.
'''

from PIL import Image

for i in range(60, 61):
    f = "../ExerciseMedia/BoltAnim/Bolt%03d.bmp"%i
    img = Image.open(f)
    newimg = Image.new('RGBA', img.size)
    for y in range(img.height):
        for x in range(img.width):
            p = img.getpixel((x, y))
            newimg.putpixel((x, y), (*p, sum(p)//3))

    newimg.save(f.replace('bmp', 'png'))
