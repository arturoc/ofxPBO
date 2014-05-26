/*
 * ofPBO.cpp
 *
 *  Created on: 08/04/2012
 *      Author: arturo
 */

#include "ofPBO.h"
#include <aio.h>

#ifndef TARGET_OPENGLES


ofPBO::ofPBO() {
	// TODO Auto-generated constructor stub

}

ofPBO::~ofPBO() {
	if(!pboIds.empty()){
		glDeleteBuffersARB(pboIds.size(), &pboIds[0]);
	}
}

void ofPBO::allocate(ofTexture & tex, int numPBOs){
	pboIds.resize(numPBOs);
    glGenBuffersARB(numPBOs, &pboIds[0]);
    int numChannels=1;
    switch(tex.getTextureData().glTypeInternal){
    case GL_LUMINANCE8:
    	numChannels = 1;
    	break;
    case GL_RGB8:
    	numChannels = 3;
    	break;
    case GL_RGBA8:
    	numChannels = 4;
    	break;
    }
    dataSize = tex.getWidth()*tex.getHeight()*numChannels;
    for(int i=0;i<(int)pboIds.size();i++){
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIds[i]);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, dataSize, 0, GL_STREAM_DRAW_ARB);
    }
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    texture = tex;
    lastDataUploaded = true;
    startThread();
}

void ofPBO::loadData(const ofPixels & pixels, bool threaded){
	if(pboIds.empty()){
		ofLogError() << "pbo not allocated";
		return;
	}

	mutex.lock();
	if(!lastDataUploaded){
		mutex.unlock();
		return;
	}else{
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); // release pointer to mapping buffer
		mutex.unlock();
	}

    indexUploading = (indexUploading + 1) % pboIds.size();

    cpu_ptr = pixels.getPixels();

	// bind PBO to update pixel values
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIds[indexUploading]);

	// map the buffer object into client's memory
	// Note that glMapBufferARB() causes sync issue.
	// If GPU is working with this buffer, glMapBufferARB() will wait(stall)
	// for GPU to finish its job. To avoid waiting (stall), you can call
	// first glBufferDataARB() with NULL pointer before glMapBufferARB().
	// If you do that, the previous data in PBO will be discarded and
	// glMapBufferARB() returns a new allocated pointer immediately
	// even if GPU is still working with the previous data.
	glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, dataSize, 0, GL_STREAM_DRAW_ARB);
	gpu_ptr = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	if(gpu_ptr)
	{
		// update data directly on the mapped buffer
		lastDataUploaded = false;
		condition.signal();
	}

}

void ofPBO::threadedFunction(){
	mutex.lock();
	while(isThreadRunning()){
		condition.wait(mutex);
		mutex.unlock();

		memcpy(gpu_ptr,cpu_ptr,dataSize);

		mutex.lock();
	    indexToUpdate = indexUploading;
		pboUpdated = true;
		lastDataUploaded = true;
	}
}

void ofPBO::updateTexture(){
	mutex.lock();
	if(pboUpdated){
		mutex.unlock();
		// bind the texture and PBO
		texture.bind();
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIds[indexToUpdate]);
		glTexSubImage2D(texture.getTextureData().textureTarget, 0, 0, 0, texture.getWidth(), texture.getHeight(), ofGetGLFormatFromInternal(texture.getTextureData().glTypeInternal), GL_UNSIGNED_BYTE, 0);
		texture.unbind();
		// it is good idea to release PBOs with ID 0 after use.
		// Once bound with 0, all pixel operations behave normal ways.
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		pboUpdated = false;
	}else{
		mutex.unlock();
	}
}
#endif
