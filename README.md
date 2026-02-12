# YOLO-Label

## Sponsors

- AIM(https://www.aimdefence.com/)
<a href="https://www.aimdefence.com/">
 <img src="https://user-images.githubusercontent.com/35001605/143895639-79b0b6b1-365f-4132-b272-f161d3ef5cb4.png" width="200">
</a>


## WHAT IS THIS?!
 Reinventing The Wheel?!!!!
 
 ![1_hfyjxxcfingbcyzcgksaiq](https://user-images.githubusercontent.com/35001605/47629997-b47aa200-db81-11e8-8873-71ae653563e0.png)

 In the world, there are many good image-labeling tools for object detection. -e.g. , ([Yolo_mark](https://github.com/AlexeyAB/Yolo_mark), [BBox-Label-Tool](https://github.com/puzzledqs/BBox-Label-Tool), [labelImg](https://github.com/tzutalin/labelImg)). 
 
But... I've reinvented one...
 
## WHY DID YOU REINVENT THE WHEEL? ARE YOU STUPID?

 When I used the pre-existing programs to annotate a training set for YOLO V3, I was sooooooooooo bored...
 
 So I thought why it is so boring??

 And I found an answer.
 
 The answer is that pre-existing programs are not **sensitive**.
 
 So I decided to make a **sensitive** image-labeling tool for object detection.
 
 ## SHOW ME YOUR SENSITIVE IMAGE-LABELING TOOL!!

 It's the **SENSITIVE** image-labeling tool for object detection!
 
![image](https://user-images.githubusercontent.com/35001605/211553495-66e81a7d-df00-44ca-82e4-966000cddbd1.png)

https://user-images.githubusercontent.com/35001605/211560039-367f27d7-63ab-4342-824e-9f47f2afbc35.mp4

![cut (2)](https://user-images.githubusercontent.com/35001605/143729909-b2da3669-020a-4769-ab1d-2646dd7bbb6b.gif)

 ## HMM... I SAW THIS DESIGN SOMEWHERE
  I refer to [the website of Joseph Redmon](https://pjreddie.com/darknet/
) who invented the [YOLO](https://www.youtube.com/watch?v=MPU2HistivI).

  ![redmon2](https://user-images.githubusercontent.com/35001605/47635529-a1270100-db98-11e8-8c03-1dcea7c77d1d.PNG)
# TUTORIAL / USAGE

## Install and Run

1. Download this project

### For Windows

2. Download [YOLOLabel_v1.2.1.zip](https://github.com/developer0hye/Yolo_Label/releases/download/v1.2.1/YoloLabel_v1.2.1.zip)

3. Unzip

4. Run YoloLabel.exe

![image](https://user-images.githubusercontent.com/35001605/111152300-e74b5680-85d3-11eb-8df7-178148548c12.png)

### For Ubuntu 22.04

2. Download [YOLOLabel_v1.2.1.tar](https://github.com/developer0hye/Yolo_Label/releases/download/v1.2.1/YoloLabel_v1.2.1.tar)

3. Unzip and download libraries
```
tar -xvf YoloLabel_v1.2.1.tar
sudo apt update
sudo apt-get install -y libgl1-mesa-dev
sudo apt-get install libxcb-*
sudo apt-get install libxkb*
```

4. Run YoloLabel.sh
```
./YoloLabel.sh
```

![image](https://user-images.githubusercontent.com/35001605/212230332-7e62bc50-7440-45c8-afc3-faebc0b31318.png)


### For macOS

2. Clone or download the source code of this repository

3. Open terminal and type command in the downloaded directory.
```console
yourMacOS:Yolo_Label you$ qmake
yourMacOS:Yolo_Label you$ make
```

4. Run YoloLabel.app/Contents/MacOS/YoloLabel in terminal or double click YoloLabel.app to run
```console
yourMacOS:Yolo_Label you$ ./YoloLabel.app/MacOS/YoloLabel
```

## Prepare Custom Dataset and Load

1. Put your .jpg, .png -images into a directory
(In this tutorial I will use the Kangarooo and the Raccoon Images. These images are in the 'Samples' folder.)

![dataset](https://user-images.githubusercontent.com/35001605/47704306-8e7afd80-dc66-11e8-9f28-13010bd2d825.PNG)

2. Put the names of the objects, each name on a separate line and save the file( .txt, .names).

![objnames](https://user-images.githubusercontent.com/35001605/47704259-75724c80-dc66-11e8-9ed1-2f84d0240ebc.PNG)

3. Run Yolo Label!

![image](https://user-images.githubusercontent.com/35001605/143729836-b2ee1188-f829-473f-aff0-d13569b3fc39.png)

4. Click the button 'Open Files' and open the folder with the images and the file('*'.names or '*'.txt) with the names of the objects.

![image](https://user-images.githubusercontent.com/35001605/211560758-f119f562-9462-4ebe-86fa-a9c169b18993.png)

5. And... Label!... Welcome to Hell... I really hate this work in the world.

This program has adopted a different labeling method from other programs that adopt **"drag and drop"** method.

To minimize wrist strain when labeling, I adopted the method **"twice left button click"** method more convenient than 

**"drag and drop"** method.

**drag and drop**

![draganddrop](https://user-images.githubusercontent.com/35001605/48674135-6fe49400-eb8c-11e8-963c-c343867b7565.gif)


**twice left button click**

![twiceleftbuttonclickmethod](https://user-images.githubusercontent.com/35001605/48674136-71ae5780-eb8c-11e8-8d8f-8cb511009491.gif)


![ezgif-5-805073516651](https://user-images.githubusercontent.com/35001605/47698872-5bc80980-dc54-11e8-8984-e3e1230eccaf.gif)

6. End

![endimage](https://user-images.githubusercontent.com/35001605/47704336-a6528180-dc66-11e8-8551-3ecb612b7353.PNG)

## SHORTCUTS

| Key | Action |
|---|:---:|
| `A` | Save and Prev Image  |
| `D,  Space` | Save and Next Image |
| `S` | Next Label <br> ![ezgif-5-f7ee77cd24c3](https://user-images.githubusercontent.com/35001605/47703190-d3049a00-dc62-11e8-846f-5bd91e98bdbc.gif)  |
| `W` | Prev Label <br> ![ezgif-5-ee915c66dad8](https://user-images.githubusercontent.com/35001605/47703191-d39d3080-dc62-11e8-800b-986ec214b80c.gif)  |
| `O` | Open Files |
| `V` | Visualize Class Name |
| `Ctrl + S` | Save |
| `Ctrl + C` | Delete all existing bounding boxes in the image |
| `Ctrl + D` | Delete current image |

| Mouse | Action |
|---|:---:|
| `Right Click` | Delete Focused Bounding Box in the image <br> ![ezgif-5-8d0fb51bec75](https://user-images.githubusercontent.com/35001605/47706913-c20d5600-dc6d-11e8-8a5c-47065f6a6416.gif) |
| `Ctrl + Double Left Click` | Change class of focused bounding box to the currently selected label (no need to remove and redraw) |
| `Wheel Down` | Save and Next Image  |
| `Wheel Up` | Save and Prev Image |

## Button Events

### Remove

It was replaced by the shortcut **Ctrl + D**.

![ezgif-2-90fb8205437e](https://user-images.githubusercontent.com/35001605/49983945-fbddb600-ffa8-11e8-9672-f7b71e4e603b.gif)

## ETC

You can access all image by moving horizontal slider bar. But when you control horizontal slider bar, the last processed image will not be saved automatically. So if you want not to lose your work, you should save before moving the horizontal slider bar.

![ezgif-5-53abf38b3387](https://user-images.githubusercontent.com/35001605/47708528-97bd9780-dc71-11e8-94f1-5ee23776d5fe.gif)

# CONCLUSIONS

I've reinvented the wheel.

![dont-reinvent-the-wheel](https://user-images.githubusercontent.com/35001605/47709289-46160c80-dc73-11e8-8ef6-6af3a3c52403.jpg)

# TO DO LISTS

Upload binary file for easy usage for windows and ubuntu

deployment for ubuntu

Image zoom
