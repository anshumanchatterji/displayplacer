//  displayplacer.c
//  Created by Jake Hilborn on 5/16/15.

#include <IOKit/graphics/IOGraphicsLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "header.h"

int main(int argc, char* argv[]) {

    listScreens();
    // printCurrentProfile();
    return 0;

}


void listScreens() {
    CGDisplayCount screenCount;
    CGGetOnlineDisplayList(INT_MAX, NULL, &screenCount); //get number of online screens and store in screenCount

    CGDirectDisplayID screenList[screenCount];
    CGGetOnlineDisplayList(INT_MAX, screenList, &screenCount);

    for (int i = 0; i < screenCount; i++) {
        CGDirectDisplayID curScreen = screenList[i];

        int curModeId;
        CGSGetCurrentDisplayMode(curScreen, &curModeId);
        modes_D4 curMode;
        CGSGetDisplayModeDescriptionOfLength(curScreen, curModeId, &curMode, 0xD4);

        char curScreenUUID[UUID_SIZE];
        CFStringGetCString(CFUUIDCreateString(kCFAllocatorDefault, CGDisplayCreateUUIDFromDisplayID(curScreen)), curScreenUUID, sizeof(curScreenUUID), kCFStringEncodingUTF8);

        int modeCount;
        modes_D4* modes;
        CopyAllDisplayModes(curScreen, &modes, &modeCount);

        for (int i = 0; i < modeCount; i++) {
            modes_D4 mode = modes[i];

            if(mode.derived.width > 1) {
                printf("%dx%d",mode.derived.width, mode.derived.height);
            }


            if (i == curModeId) {
                // printf(" <-- current mode");
            }

            printf("\n");
        }
        // printf("\n");
    }
}

void printCurrentProfile() {
    CGDisplayCount screenCount;
    CGGetOnlineDisplayList(INT_MAX, NULL, &screenCount); //get number of online screens and store in screenCount

    CGDirectDisplayID screenList[screenCount];
    CGGetOnlineDisplayList(INT_MAX, screenList, &screenCount);

    ScreenConfig screenConfigs[screenCount];
    for (int i = 0; i < screenCount; i++) {
        screenConfigs[i].id = screenList[i];
        screenConfigs[i].mirrorCount = 0;
    }

    for (int i = 0; i < screenCount; i++) {
        if (CGDisplayIsInMirrorSet(screenConfigs[i].id) && CGDisplayMirrorsDisplay(screenConfigs[i].id) != 0) { //this screen is a secondary screen in a mirroring set
            int primaryScreenId = CGDisplayMirrorsDisplay(screenConfigs[i].id);
            int secondaryScreenId = screenConfigs[i].id;

            for (int j = 0; j < screenCount; j++) {
                if (screenConfigs[j].id == primaryScreenId) {
                    screenConfigs[j].mirrors[screenConfigs[j].mirrorCount] = secondaryScreenId;
                    screenConfigs[j].mirrorCount++;
                }
            }

            screenConfigs[i].id = -1;
        }
    }

    printf("Execute the command below to set your screens to the current arrangement:\n\n");
    printf("displayplacer");
    for (int i = 0; i < screenCount; i++) {
        ScreenConfig curScreen = screenConfigs[i];

        if (curScreen.id == -1) { //earlier we set this to -1 since it will be represented as a mirror on output
            continue;
        }

        int curModeId;
        CGSGetCurrentDisplayMode(curScreen.id, &curModeId);
        modes_D4 curMode;
        CGSGetDisplayModeDescriptionOfLength(curScreen.id, curModeId, &curMode, 0xD4);

        char hz[8]; //hz:999 \0
        strlcpy(hz, "", sizeof(hz)); //most displays do not have hz option
        if (curMode.derived.freq) {
            snprintf(hz, sizeof(hz), "hz:%i ", curMode.derived.freq);
        }

        char* scaling = (curMode.derived.density == 2.0) ? "on" : "off";

        char mirrors[(UUID_SIZE + 1) * MIRROR_MAX + 1];
        strlcpy(mirrors, "", sizeof(mirrors));
        for (int j = 0; j < curScreen.mirrorCount; j++) {
            char mirrorUUID[UUID_SIZE];
            CFStringGetCString(CFUUIDCreateString(kCFAllocatorDefault, CGDisplayCreateUUIDFromDisplayID(curScreen.mirrors[j])), mirrorUUID, sizeof(mirrorUUID), kCFStringEncodingUTF8);

            strlcat(mirrors, "+", sizeof(mirrors));
            strlcat(mirrors, mirrorUUID, sizeof(mirrors));
        }

        char curScreenUUID[UUID_SIZE];
        CFStringGetCString(CFUUIDCreateString(kCFAllocatorDefault, CGDisplayCreateUUIDFromDisplayID(curScreen.id)), curScreenUUID, sizeof(curScreenUUID), kCFStringEncodingUTF8);

        printf(" \"id:%s%s res:%ix%i %scolor_depth:%i scaling:%s origin:(%i,%i) degree:%i\"", curScreenUUID, mirrors, (int) CGDisplayPixelsWide(curScreen.id), (int) CGDisplayPixelsHigh(curScreen.id), hz, curMode.derived.depth, scaling, (int) CGDisplayBounds(curScreen.id).origin.x, (int) CGDisplayBounds(curScreen.id).origin.y, (int) CGDisplayRotation(curScreen.id));
    }
    printf("\n");
}

CGDirectDisplayID convertUUIDtoID(char* uuid) {
    if (strstr(uuid, "-") == NULL) { //contextual screen id
        return atoi(uuid);
    }

    CFStringRef uuidStringRef = CFStringCreateWithCString(kCFAllocatorDefault, uuid, kCFStringEncodingUTF8);
    CFUUIDRef uuidRef = CFUUIDCreateFromString(kCFAllocatorDefault, uuidStringRef);
    return CGDisplayGetDisplayIDFromUUID(uuidRef);
}

bool validateScreenOnline(CGDirectDisplayID onlineDisplayList[], int screenCount, CGDirectDisplayID screenId, char* screenUUID) {
    for (int i = 0; i < screenCount; i++) {
        if (onlineDisplayList[i] == screenId) {
            return true;
        }
    }

    fprintf(stderr, "Unable to find screen %s - skipping changes for that screen\n", screenUUID);
    return false;
}

bool rotateScreen(CGDirectDisplayID screenId, char* screenUUID, int degree) {
    io_service_t service = CGDisplayIOServicePort(screenId);
    IOOptionBits options;

    switch(degree) {
        default:
            options = (0x00000400 | (kIOScaleRotate0)  << 16);
            break;
        case 90:
            options = (0x00000400 | (kIOScaleRotate90)  << 16);
            break;
        case 180:
            options = (0x00000400 | (kIOScaleRotate180)  << 16);
            break;
        case 270:
            options = (0x00000400 | (kIOScaleRotate270)  << 16);
            break;
    }

    int retVal = IOServiceRequestProbe(service, options);

    if (retVal != 0) {
        fprintf(stderr, "Error rotating screen %s\n", screenUUID);
        return false;
    }

    return true;
}

bool configureMirror(CGDisplayConfigRef configRef, CGDirectDisplayID primaryScreenId, char* primaryScreenUUID, CGDirectDisplayID mirrorScreenId, char* mirrorScreenUUID) {
    int retVal = CGConfigureDisplayMirrorOfDisplay(configRef, mirrorScreenId, primaryScreenId);

    if (retVal != 0) {
        fprintf(stderr, "Error making the secondary screen %s mirror the primary screen %s\n", mirrorScreenUUID, primaryScreenUUID);
        return false;
    }

    return true;
}

bool configureResolution(CGDisplayConfigRef configRef, CGDirectDisplayID screenId, char* screenUUID, int width, int height, int hz, int depth, bool scaled, int modeNum) {
    int modeCount;
    modes_D4* modes;

    if (modeNum != -1) { //user specified modeNum instead of height/width/hz
        CGSConfigureDisplayMode(configRef, screenId, modeNum);
        return true;
    }

    CopyAllDisplayModes(screenId, &modes, &modeCount);

    modes_D4 bestMode = modes[0];
    bool modeFound = false;

    //loop through all modes looking for one that matches user input params
    for (int i = 0; i < modeCount; i++) {
        modes_D4 curMode = modes[i];
        
        //prioritize exact matches of user input params
        if (curMode.derived.width != width) continue;
        if (curMode.derived.height != height) continue;
        if (hz && curMode.derived.freq != hz) continue;
        if (depth && curMode.derived.depth != depth) continue;
        if (scaled && curMode.derived.density != 2.0) continue;

        if (!modeFound) {
            modeFound = true;
            bestMode = curMode;
        }

        if (curMode.derived.freq > bestMode.derived.freq || (curMode.derived.freq == bestMode.derived.freq && curMode.derived.depth > bestMode.derived.depth)) {
            bestMode = curMode;
        }
    }

    if (modeFound) {
        CGSConfigureDisplayMode(configRef, screenId, bestMode.derived.mode);
        return true;
    }

    fprintf(stderr, "Screen ID %s: could not find res:%ix%i", screenUUID, width, height);
    if (hz) {
        fprintf(stderr, " hz:%i", hz);
    }
    if (depth) {
        fprintf(stderr, " color_depth:%i", depth);
    }
    char* scalingString = (scaled == 2.0) ? "on" : "off";
    fprintf(stderr, " scaling:%s", scalingString);

    fprintf(stderr, "\n");

    return false;
}

bool configureOrigin(CGDisplayConfigRef configRef, CGDirectDisplayID screenId, char* screenUUID, int x, int y) {
    int retVal = CGConfigureDisplayOrigin(configRef, screenId, x, y);

    if (retVal != 0) {
        fprintf(stderr, "Error moving screen %s to %ix%i\n", screenUUID, x, y);
        return false;
    }

    return true;
}
