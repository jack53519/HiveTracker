// Main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

float distance_point(Point a, Point b) {
    float distance;
    int x_dist = max(a.x, b.x) - min(a.x, b.x);
    int y_dist = max(a.y, b.y) - min(a.y, b.y);
    distance = float(x_dist * x_dist) + float(y_dist * y_dist);
    return distance;
}

Mat element3x3 = getStructuringElement(MORPH_RECT, Size(3, 3));
Mat element5x5 = getStructuringElement(MORPH_RECT, Size(5, 5));
Mat element7x7 = getStructuringElement(MORPH_RECT, Size(7, 7));


vector<Bee> beeArray;
vector<bool> tagInUse(1000, 0);
VideoCapture cap;
Ptr<BackgroundSubtractor> bgSubKNN;
fpsCounter fps;

int main(int argc, const char * argv[]) {
    cap = VideoCapture("/Users/Jack/Desktop/OpenCV Bees/bees.mp4");
    bgSubKNN = createBackgroundSubtractorKNN(10000, 10000.0, true);
    Mat imgRaw;
    cap.read(imgRaw);
    const int scale = int(float(imgRaw.cols) / 500.0f);
    Counter beeCounter(beeArray, imgRaw.cols, imgRaw.rows);
    cap.set(CAP_PROP_FPS, 15);
    cout << cap.get(CAP_PROP_FPS) << endl;
    while (cap.isOpened() && waitKey(1) != 27) {
        cap.read(imgRaw);
        
        Mat image;
        resize(imgRaw, image, imgRaw.size() / scale);
        
        Mat imgThresh;
        bgSubKNN->apply(image.clone(), imgThresh);
        threshold(imgThresh, imgThresh, 254, 255, THRESH_BINARY);
        
        resize(imgThresh, imgThresh, imgRaw.size() / (scale/2));
        //imshow("Background", imgThresh);
        
        resize(imgRaw, image, imgRaw.size() / (scale/2));
        dilate(imgThresh, imgThresh, element5x5);
        erode(imgThresh, imgThresh, element3x3);
        //imshow("Dilation", imgThresh);
        
        Mat maskFilled;
        imgThresh.copyTo(maskFilled);
        floodFill(maskFilled, Point(0, 0), Scalar(255));
        floodFill(maskFilled, Point(maskFilled.cols - 1, 0), Scalar(255));
        floodFill(maskFilled, Point(0, maskFilled.rows - 1), Scalar(255));
        floodFill(maskFilled, Point(maskFilled.cols - 1, maskFilled.rows - 1), Scalar(255));
        bitwise_not(maskFilled, maskFilled);
        bitwise_or(maskFilled, imgThresh, maskFilled);
        //erode(maskFilled, maskFilled, element5x5);
        //imshow("Filled Mask", maskFilled);
        
        Mat maskYellow, maskBlack, imgHsv;
        cvtColor(image, imgHsv, COLOR_BGR2HSV);
        Scalar lowerYellow = Scalar(12, 65, 40);
        Scalar upperYellow = Scalar(24, 255, 250);
        Scalar lowerBlack = Scalar(0, 0, 0);
        Scalar upperBlack = Scalar(255, 255, 40);
        inRange(imgHsv, lowerYellow, upperYellow, maskYellow);
        inRange(imgHsv, lowerBlack, upperBlack, maskBlack);
        Mat maskComb;
        bitwise_or(maskYellow, maskBlack, maskComb, maskFilled);
        //Mat output;
        //bitwise_and(image, image, output, maskComb);
        //imshow("Background", output);
        Mat output1;
        imshow("1", maskComb);
        erode(maskComb, output1, element7x7);
        imshow("2", output1);
        dilate(maskComb, maskComb, element5x5);
        imshow("3", maskComb);
        
        Mat maskCombFill;
        maskComb.copyTo(maskCombFill);
        floodFill(maskCombFill, Point(0, 0), Scalar(255));
        bitwise_not(maskCombFill, maskCombFill);
        bitwise_or(maskCombFill, maskComb, maskCombFill);
        //imshow("Combined Mask", maskCombFill);
        resize(maskCombFill, maskCombFill, imgRaw.size());
        //imshow("Combined Mask", maskCombFill);
        //Mat markers = maskCombFill.clone();
        //erode(markers, markers, element7x7);
        //erode(markers, markers, element7x7);
        //dilate(markers, markers, element7x7);
        //erode(markers, markers, element3x3);
        //imshow("Eroded", markers);
        //maskCombFill = imgThresh.clone();
        
        
        vector<vector<Point> > contours;
        vector<Vec4i> hierarchy;
        findContours(maskCombFill, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        
        image = imgRaw.clone();
        
        vector<vector<Point> > contoursReduced;
        for (int i = 0; i < contours.size(); i++)
        {
            if (contours[i].size() > 160/(scale*scale) && contours[i].size() < 4800/(scale*scale))
            {
                contoursReduced.push_back(contours[i]);
            }
        }
        
        
        vector<Point2d> contourCenterArray(contoursReduced.size());
        for (int i = 0; i < contoursReduced.size(); i++) {
            Moments moment = moments(contoursReduced[i], false);
            Point2d center = Point2d(moment.m10 / moment.m00, moment.m01 / moment.m00);
            contourCenterArray.push_back(center);
            //putText(output, to_string(i), Point2d(center.x + 5, center.y - 10), 3, 1, Scalar(255,0,0));
        }
        
        Mat output;
        output = image.clone();
        vector<vector<double>> distances(beeArray.size(), vector<double>(contoursReduced.size(),-100000.0));
        vector<int> contourUsed(contoursReduced.size(), -1);
        for (int i = 0; i < beeArray.size(); i++) {
            for (int j = 0; j < contoursReduced.size(); j++) {
                distances[i][j] = pointPolygonTest(beeArray[i].getPredictionArea(), contourCenterArray[j], true); //Distance between current contour and current bee prediction location, positive numbers are inside
                //
                if (distances[i][j] >= 0) {
                    //cout << "Bee: " << i << ", Contour: " << j << ", Distance: " << distances[i][j] << endl;
                    if (contourUsed[j] == -1) { //First Bee to use this contour
                        contourUsed[j] = beeArray[i].getTag();
                        beeArray[i].updateBee(contoursReduced[j]);
                    }
                    else { //Two Bees share same contour
                        
                        //contourUsed[j] = beeArray[i].getTag();
                        //beeArray[i].updateBee(contoursReduced[j]);
                        cout << "Contour: " << j << endl;
                    }
                    break;
                }
            }
        }
        
        vector<double> closestBee(contoursReduced.size(), -10000.0);
        vector<int> closestBeeIndex(contoursReduced.size(), 0);
        for (int i = 0; i < contoursReduced.size(); i++) {
            for (int j = 0; j < beeArray.size(); j++) {
                if (distances[j][i] > closestBee[i] && !beeArray[j].beeUpdated()) {
                    //cout << "Bee: " << j << ", Contour: " << i << ", Distance: " << distances[j][i] << endl;
                    closestBee[i] = distances[j][i];
                    closestBeeIndex[i] = j;
                }
            }
        }
        
        //
        if (beeArray.size() > 0) {
            vector<bool> completedIndex(closestBeeIndex.size(), 0); //Bees array index that have been search, this is for a speed improvement
            for (int i = 0; i < contoursReduced.size(); i++) {
                if (completedIndex[i] == 0) {
                    int currentBeeIndex = closestBeeIndex[i]; //Bees array index
                    int closestContourIndex = i; //Contour array index
                    double value = closestBee[i]; //Distance from current contour to diffrent bees
                    for (int j = i + 1; j < contoursReduced.size(); j++) {
                        if (closestBeeIndex[j] == currentBeeIndex) { //
                            completedIndex[j] = 1;
                            value = max(value, closestBee[j]);
                            if (value == closestBee[j]) {
                                closestContourIndex = j;
                            }
                        }
                    }
                    if (value > -1* beeArray[currentBeeIndex].getUncertainty()) {
                        beeArray[currentBeeIndex].updateBee(contoursReduced[closestContourIndex]);
                        contourUsed[closestContourIndex] = 1;
                    }
                }
            }
        }
        
        //If a contour is still present and an appropriate bee cannot be matched to it, create a new Bee object
        for (int i = 0; i < contoursReduced.size(); i++) {
            if (contourUsed[i] == -1) { //If the contour has not yet been used
                for (int tag = 0; tag < 1000; tag++) { //Find the smallest tag that is free
                    if (!tagInUse[tag]) {
                        tagInUse[tag] = 1; //Set tag to being used
                        Bee newBee = Bee(tag, contoursReduced[i]); //Create new Bee object with contour and tag
                        beeArray.push_back(newBee); //Place Bee object into the array of bees
                        break; //Once a tag is found break out of tag loop
                    }
                }
            }
        }
        
        //Delete any bees that have not been updated for a certain amount of frames (Amount defined in Bee class)
        for (int i = 0; i<beeArray.size(); i++) {
            if (beeArray[i].endFrame()) { //Returns true if the bee has been inactive. See Bee class for full description
                tagInUse[beeArray[i].getTag()] = 0; //Free the tag so it can be used by other Bee objects
                beeArray.erase(beeArray.begin() + i); //Erase the Bee at location i from the Bees array
                i--; //Go back a memory location because an object will take the place of the erased one
            }
        }
        beeArray.shrink_to_fit(); //Reduce the memory size for the Bee array
        
        for (int i = 0; i<beeArray.size(); i++) {
            output = beeArray[i].printBee(output); //Print all Bees
        }
        
        beeCounter.updateCounter();
        fps.updateFps();
        output = fps.printFPS(output);
        output = beeCounter.drawCounter(output);
        
        //imshow("Output", output);
        
        while(waitKey(0) != 32);
    }
    return 0;
}


