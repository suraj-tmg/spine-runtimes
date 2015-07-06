/******************************************************************************
 * Spine Runtimes Software License
 * Version 2.3
 * 
 * Copyright (c) 2013-2015, Esoteric Software
 * All rights reserved.
 * 
 * You are granted a perpetual, non-exclusive, non-sublicensable and
 * non-transferable license to use, install, execute and perform the Spine
 * Runtimes Software (the "Software") and derivative works solely for personal
 * or internal use. Without the written permission of Esoteric Software (see
 * Section 2 of the Spine Software License Agreement), you may not (a) modify,
 * translate, adapt or otherwise create derivative works, improvements of the
 * Software or develop new applications using the Software or (b) remove,
 * delete, alter or obscure any trademarks or any copyright, trademark, patent
 * or other intellectual property or proprietary rights notices on or in the
 * Software, including any copy thereof. Redistributions in binary or source
 * form must include this license and terms.
 * 
 * THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <spine/SkeletonRenderer.h>
#include <spine/spine-cocos2dx.h>
#include <spine/extension.h>
#include <spine/PolygonBatch.h>
#include <algorithm>

USING_NS_CC;
using std::min;
using std::max;

namespace spine {

static const int quadTriangles[6] = {0, 1, 2, 2, 3, 0};

SkeletonRenderer* SkeletonRenderer::createWithData (spSkeletonData* skeletonData, bool ownsSkeletonData) {
	SkeletonRenderer* node = new SkeletonRenderer(skeletonData, ownsSkeletonData);
	node->autorelease();
	return node;
}

SkeletonRenderer* SkeletonRenderer::createWithFile (const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
	SkeletonRenderer* node = new SkeletonRenderer(skeletonDataFile, atlas, scale);
	node->autorelease();
	return node;
}

SkeletonRenderer* SkeletonRenderer::createWithFile (const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
	SkeletonRenderer* node = new SkeletonRenderer(skeletonDataFile, atlasFile, scale);
	node->autorelease();
	return node;
}

void SkeletonRenderer::initialize () {
	_atlas = 0;
	_debugSlots = false;
	_debugBones = false;
	_timeScale = 1;

	_worldVertices = MALLOC(float, 1000); // Max number of vertices per mesh.

	_batch = PolygonBatch::createWithCapacity(2000); // Max number of vertices and triangles per batch.
	_batch->retain();

	_blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;
	setOpacityModifyRGB(true);

	setGLProgram(ShaderCache::getInstance()->getGLProgram(GLProgram::SHADER_NAME_POSITION_TEXTURE_COLOR));
}

void SkeletonRenderer::setSkeletonData (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	_skeleton = spSkeleton_create(skeletonData);
	_ownsSkeletonData = ownsSkeletonData;
}

SkeletonRenderer::SkeletonRenderer () {
}

SkeletonRenderer::SkeletonRenderer (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	initWithData(skeletonData, ownsSkeletonData);
}

SkeletonRenderer::SkeletonRenderer (const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
	initWithFile(skeletonDataFile, atlas, scale);
}

SkeletonRenderer::SkeletonRenderer (const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
	initWithFile(skeletonDataFile, atlasFile, scale);
}

SkeletonRenderer::~SkeletonRenderer () {
	if (_ownsSkeletonData) spSkeletonData_dispose(_skeleton->data);
	if (_atlas) spAtlas_dispose(_atlas);
	spSkeleton_dispose(_skeleton);
	_batch->release();
	FREE(_worldVertices);
    
    
    std::map<std::string,pageStatus>::iterator it = _equipMap.begin();
    while(it!=_equipMap.end())
    {
        this->removeAttachment(it->second.pageAttachment);
        ++it;
    }
    _equipMap.clear();
    
}

void SkeletonRenderer::initWithData (spSkeletonData* skeletonData, bool ownsSkeletonData) {
	setSkeletonData(skeletonData, ownsSkeletonData);

	initialize();
}

void SkeletonRenderer::initWithFile (const std::string& skeletonDataFile, spAtlas* atlas, float scale) {
	spSkeletonJson* json = spSkeletonJson_create(atlas);
	json->scale = scale;
    _scale = scale;
	spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile.c_str());
	CCASSERT(skeletonData, json->error ? json->error : "Error reading skeleton data.");
	spSkeletonJson_dispose(json);

	setSkeletonData(skeletonData, true);

	initialize();
}

void SkeletonRenderer::initWithFile (const std::string& skeletonDataFile, const std::string& atlasFile, float scale) {
	_atlas = spAtlas_createFromFile(atlasFile.c_str(), 0);
	CCASSERT(_atlas, "Error reading atlas file.");

	spSkeletonJson* json = spSkeletonJson_create(_atlas);
	json->scale = scale;
    _scale = scale;
	spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile.c_str());
	CCASSERT(skeletonData, json->error ? json->error : "Error reading skeleton data file.");
	spSkeletonJson_dispose(json);

	setSkeletonData(skeletonData, true);

	initialize();
}


void SkeletonRenderer::update (float deltaTime) {
	spSkeleton_update(_skeleton, deltaTime * _timeScale);
}

void SkeletonRenderer::draw (Renderer* renderer, const Mat4& transform, uint32_t transformFlags) {
	_drawCommand.init(_globalZOrder);
	_drawCommand.func = CC_CALLBACK_0(SkeletonRenderer::drawSkeleton, this, transform, transformFlags);
	renderer->addCommand(&_drawCommand);
}
    
void SkeletonRenderer::reset(std::string attachmentNameToReset)
{
    float* uvs = nullptr;
	int verticesCount = 0;
	const int* triangles = nullptr;
	int trianglesCount = 0;
	float r = 0, g = 0, b = 0, a = 0,x = 0.0f, y = 0.0f;
    
    for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
		spSlot* slot = _skeleton->drawOrder[i];
		if (!slot->attachment) continue;
		Texture2D *texture = nullptr;
		switch (slot->attachment->type) {
            case SP_ATTACHMENT_REGION: {
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                std::string attachmentName = getCharacterName(attachment);
                if(attachmentNameToReset.compare(attachmentName) == 0)
                {
                    if(_attachmentMap.find(attachmentName) != _attachmentMap.end())
                    {
                        //Get attachment pointer from map.
                        spRegionAttachment *_tmpAttachment = _attachmentMap[attachmentName];
                       
                        //Remove entry from equipMap.
                        std::map<std::string,pageStatus>::iterator it;
                        it = _equipMap.find (attachmentName);
                        _equipMap.erase (it);
                       
                        //Remove attachment ,releases texture also
                        this->removeAttachment(attachment);
                       
                        //Set the saved attachment to slot attachment pointer.
                        slot->attachment = (spAttachment*)_tmpAttachment;
                    }
                }
                break;
            }
            default:
                break;
        }
        
    }
}
    
    
void SkeletonRenderer::reset()
{
    float* uvs = nullptr;
	int verticesCount = 0;
	const int* triangles = nullptr;
	int trianglesCount = 0;
	float r = 0, g = 0, b = 0, a = 0,x = 0.0f, y = 0.0f;
    
    for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
		spSlot* slot = _skeleton->drawOrder[i];
		if (!slot->attachment) continue;
		Texture2D *texture = nullptr;
		switch (slot->attachment->type) {
            case SP_ATTACHMENT_REGION: {
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                std::string attachmentName = getCharacterName(attachment);
               
                if(_attachmentMap.find(attachmentName) == _attachmentMap.end())
                {
                    spRegionAttachment *_tmpAttachment = _attachmentMap[attachmentName];
                   
                    //Remove entry from equipMap.
                    std::map<std::string,pageStatus>::iterator it;
                    it = _equipMap.find (attachmentName);
                    _equipMap.erase (it);
                    
                    //Remove attachment ,releases texture also
                    this->removeAttachment(attachment);
                    
                    //Set the saved attachment to slot attachment pointer.
                    slot->attachment = (spAttachment*)_tmpAttachment;
                }
                
                break;
            }
            default:
                break;
        }
        
    }

}

void SkeletonRenderer::setVisibilityForAttachment(std::string attachmentName,bool value)
{
    if(_propertiesMap.find(attachmentName) == _propertiesMap.end())
    {
        attachmentProperties prop;
        prop.visibility = value;
        prop.isColorSet = false;
        _propertiesMap[attachmentName] = prop;
    }
    else
    {
        _propertiesMap[attachmentName].visibility = value;
        _propertiesMap[attachmentName].isColorSet = _propertiesMap[attachmentName].isColorSet == true ? true : false;
    }
}
    
void SkeletonRenderer::setColorForAttachment(std::string attachmentName,cocos2d::ccColor4B col)
{
    if(_propertiesMap.find(attachmentName) == _propertiesMap.end())
    {
        attachmentProperties prop;
        prop.isColorSet = true;
        prop.r = col.r;
        prop.g = col.g;
        prop.b = col.b;
        prop.a = col.a;
        prop.visibility = true;
        
        _propertiesMap[attachmentName] = prop;
    }
    else
    {
        _propertiesMap[attachmentName].isColorSet = true;
        _propertiesMap[attachmentName].r = col.r;
        _propertiesMap[attachmentName].g = col.g;
        _propertiesMap[attachmentName].b = col.b;
        _propertiesMap[attachmentName].a = col.a;
    }
}

bool SkeletonRenderer::isAttachmentVisible(std::string attachmentName)
{
   return  _propertiesMap.find(attachmentName) == _propertiesMap.end() ? true : _propertiesMap[attachmentName].visibility;
}
    
void SkeletonRenderer::setAttachmentPng(std::string attachmentName,std::string pngName)
{
    float* uvs = nullptr;
	int verticesCount = 0;
	const int* triangles = nullptr;
	int trianglesCount = 0;
	float r = 0, g = 0, b = 0, a = 0,x = 0.0f, y = 0.0f;
    
    std::map<std::string,bool> _avoidDuplication;
    
    for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
		spSlot* slot = _skeleton->drawOrder[i];
		if (!slot->attachment) continue;
		Texture2D *texture = nullptr;
		switch (slot->attachment->type) {
            case SP_ATTACHMENT_REGION: {
                spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
                std::string currentAttachmentName = getCharacterName(attachment);
                spRegionAttachment* newAttachment;
                
                if(attachmentName.compare(currentAttachmentName) == 0)
                {
                    if(_avoidDuplication.find(currentAttachmentName) != _avoidDuplication.end())
                    {
                        return;
                    }
                    _avoidDuplication[currentAttachmentName] = true;
                    
                    int lastindex = pngName.find_last_of(".");
                    std::string rawName = pngName.substr(0, lastindex);
                    rawName = rawName.substr(rawName.find_last_of("/\\") + 1);
                    
                    Texture2D * tex = NULL;
                    
                    if(_equipMap.find(attachmentName) == _equipMap.end() || _equipMap[attachmentName].pageState == false)
                    {
                        //ATLAS PAGE DOES NOT EXIST : Page does not exist create it and update map.
                        //Save the original state of the attachment in Attachment map .
                        
                        //Save the pointer of the original attachment.
                        _attachmentMap[currentAttachmentName] = (spRegionAttachment*)slot->attachment;
                        void* _tmpRenderObject = attachment->rendererObject; //spAtlasRegion is the render object.
                        
                        //New attachment created.Should point to old atlas region .
                        newAttachment = this->createAttachmentWithPng(attachmentName, pngName);
                        
                        
                        //Point the current slot to new attachment created.
                        slot->attachment = (spAttachment*)newAttachment;
                        attachment = newAttachment;
                        
                        //equipMap[attachmentName].pagePointer = page;
                        _equipMap[attachmentName].pageState = true;
                        _equipMap[attachmentName].pageAttachment = newAttachment;
                    }
                    else
                    {
                        //If attachment exist in the map
                        spAtlasPage * attachmentPage = ((spAtlasRegion*)attachment->rendererObject)->page;
                        Texture2D * prevTexture = (Texture2D*)attachmentPage->rendererObject;
                        if(prevTexture)
                        {
                            prevTexture->release();
                            attachmentPage->rendererObject = NULL;
                        }
                        _spAtlasPage_createTexture(attachmentPage,pngName.c_str());
                    }
                    
                    tex = (Texture2D*)((spAtlasRegion*)attachment->rendererObject)->page->rendererObject;
                    spRegionAttachment* regionAttachement = this->getAttachmentOffset(rawName);
                    
                    attachment->x = regionAttachement->x;
                    attachment->y = regionAttachement->y;
                    attachment->scaleX = regionAttachement->scaleX;
                    attachment->scaleY = regionAttachement->scaleY;
                    attachment->rotation = regionAttachement->rotation;
                    attachment->width = regionAttachement->width;
                    attachment->height = regionAttachement->height;
                    attachment->r = regionAttachement->r;
                    attachment->g = regionAttachement->g;
                    attachment->b = regionAttachement->b;
                    attachment->a = regionAttachement->a;
                    
                    attachment->regionOffsetX = 0;
                    attachment->regionOffsetY = 0;
                    attachment->regionWidth = tex->getPixelsWide();
                    attachment->regionHeight = tex->getPixelsHigh();
                    attachment->regionOriginalWidth = tex->getPixelsWide();
                    attachment->regionOriginalHeight = tex->getPixelsHigh();
                    attachment->width = attachment->regionWidth*this->_scale;   //Multiply the scale that was set during initialisation.
                    attachment->height = attachment->regionHeight*this->_scale;
                    spAtlasRegion* atlasRegion = (spAtlasRegion*)attachment->rendererObject;
                    spRegionAttachment_setUVs(attachment,0,0,1,1,atlasRegion->rotate);
                    
                    spRegionAttachment_updateOffset(attachment);
                    
                    
                    break;
                }
            }
                
        }
        
    }

    
    
}
    
    
spRegionAttachment* SkeletonRenderer::createAttachmentWithPng(std::string attachmentName,std::string pngName)
{
    spRegionAttachment * newAttachment =  spRegionAttachment_create(attachmentName.c_str()); //Mem alloc
    newAttachment->rendererObject = spAtlasRegion_create();
    MALLOC_STR(((spAtlasRegion*)newAttachment->rendererObject)->name, attachmentName.c_str());
    
    spAtlasPage * page = spAtlasPage_create_with_filename(pngName.c_str()); //Mem alloc
    ((spAtlasRegion*)newAttachment->rendererObject)->page = page; //creatin page.Page contains new texture
    
    return newAttachment;
    
}
    
void SkeletonRenderer::removeAttachment(spRegionAttachment *attachment)
{
    //Remove texture
    if((spAtlasRegion*)attachment && (spAtlasRegion*)attachment->rendererObject)
    {
        spAtlasPage * pageToRemove = ((spAtlasRegion*)attachment->rendererObject)->page;
        if(pageToRemove)
            spAtlasPage_dispose_with_filename(pageToRemove);
    }
    
    //Remove atlas region
    if((spAtlasRegion*)attachment && (spAtlasRegion*)attachment->rendererObject)
        spAtlasRegion_dispose((spAtlasRegion*)attachment->rendererObject);
    
    //Remove the attachment that was created.
    if((spAtlasRegion*)attachment)
        _spRegionAttachment_dispose((spAttachment*)attachment);
}


    
void SkeletonRenderer::drawSkeleton (const Mat4 &transform, uint32_t transformFlags) {
	getGLProgramState()->apply(transform);

	Color3B nodeColor = getColor();
	_skeleton->r = nodeColor.r / (float)255;
	_skeleton->g = nodeColor.g / (float)255;
	_skeleton->b = nodeColor.b / (float)255;
	_skeleton->a = getDisplayedOpacity() / (float)255;

	int blendMode = -1;
	Color4B color;
    float* uvs = nullptr;
	int verticesCount = 0;
	const int* triangles = nullptr;
	int trianglesCount = 0;
	float r = 0, g = 0, b = 0, a = 0;
	for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
		spSlot* slot = _skeleton->drawOrder[i];
		if (!slot->attachment) continue;
		Texture2D *texture = nullptr;
		switch (slot->attachment->type) {
		case SP_ATTACHMENT_REGION: {
			spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
			
            if( _propertiesMap.find(getCharacterName(attachment)) != _propertiesMap.end())
            {
                if(!_propertiesMap[getCharacterName(attachment)].visibility)
                {
                    continue;
                }
                else if(_propertiesMap[getCharacterName(attachment)].isColorSet)
                {
                  attachment->r =  _propertiesMap[getCharacterName(attachment)].r ;
                  attachment->g =  _propertiesMap[getCharacterName(attachment)].g;
                  attachment->b =  _propertiesMap[getCharacterName(attachment)].b;
//                  attachment->a =  _propertiesMap[getCharacterName(attachment)].a;
                }
            }
            spRegionAttachment_computeWorldVertices(attachment, slot->bone, _worldVertices);
			texture = getTexture(attachment);
                uvs = attachment->uvs;
                verticesCount = 8 ;
                triangles = quadTriangles;
                trianglesCount = 6;
                r = attachment->r;
                g = attachment->g;
                b = attachment->b;
                a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_MESH: {
			spMeshAttachment* attachment = (spMeshAttachment*)slot->attachment;
			spMeshAttachment_computeWorldVertices(attachment, slot, _worldVertices);
			texture = getTexture(attachment);
			uvs = attachment->uvs;
			verticesCount = attachment->verticesCount;
			triangles = attachment->triangles;
			trianglesCount = attachment->trianglesCount;
			r = attachment->r;
			g = attachment->g;
			b = attachment->b;
			a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_SKINNED_MESH: {
			spSkinnedMeshAttachment* attachment = (spSkinnedMeshAttachment*)slot->attachment;
			spSkinnedMeshAttachment_computeWorldVertices(attachment, slot, _worldVertices);
			texture = getTexture(attachment);
			uvs = attachment->uvs;
			verticesCount = attachment->uvsCount;
			triangles = attachment->triangles;
			trianglesCount = attachment->trianglesCount;
			r = attachment->r;
			g = attachment->g;
			b = attachment->b;
			a = attachment->a;
			break;
		}
		default: ;
		} 
		if (texture) {
			if (slot->data->blendMode != blendMode) {
				_batch->flush();
				blendMode = slot->data->blendMode;
				switch (slot->data->blendMode) {
				case SP_BLEND_MODE_ADDITIVE:
					GL::blendFunc(_premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA, GL_ONE);
					break;
				case SP_BLEND_MODE_MULTIPLY:
					GL::blendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case SP_BLEND_MODE_SCREEN:
					GL::blendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
					break;
				default:
					GL::blendFunc(_blendFunc.src, _blendFunc.dst);
				}
			}
			color.a = _skeleton->a * slot->a * a * 255;
			float multiplier = _premultipliedAlpha ? color.a : 255;
			color.r = _skeleton->r * slot->r * r * multiplier;
			color.g = _skeleton->g * slot->g * g * multiplier;
			color.b = _skeleton->b * slot->b * b * multiplier;
			_batch->add(texture, _worldVertices, uvs, verticesCount, triangles, trianglesCount, &color);
		}
	}
	_batch->flush();

	if (_debugSlots || _debugBones) {
		Director* director = Director::getInstance();
		director->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
		director->loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW, transform);

		if (_debugSlots) {
			// Slots.
			DrawPrimitives::setDrawColor4B(0, 0, 255, 255);
			glLineWidth(1);
			Vec2 points[4];
			V3F_C4B_T2F_Quad quad;
			for (int i = 0, n = _skeleton->slotsCount; i < n; i++) {
				spSlot* slot = _skeleton->drawOrder[i];
				if (!slot->attachment || slot->attachment->type != SP_ATTACHMENT_REGION) continue;
				spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
				spRegionAttachment_computeWorldVertices(attachment, slot->bone, _worldVertices);
				points[0] = Vec2(_worldVertices[0], _worldVertices[1]);
				points[1] = Vec2(_worldVertices[2], _worldVertices[3]);
				points[2] = Vec2(_worldVertices[4], _worldVertices[5]);
				points[3] = Vec2(_worldVertices[6], _worldVertices[7]);
				DrawPrimitives::drawPoly(points, 4, true);
			}
		}
		if (_debugBones) {
			// Bone lengths.
			glLineWidth(2);
			DrawPrimitives::setDrawColor4B(255, 0, 0, 255);
			for (int i = 0, n = _skeleton->bonesCount; i < n; i++) {
				spBone *bone = _skeleton->bones[i];
				float x = bone->data->length * bone->m00 + bone->worldX;
				float y = bone->data->length * bone->m10 + bone->worldY;
				DrawPrimitives::drawLine(Vec2(bone->worldX, bone->worldY), Vec2(x, y));
			}
			// Bone origins.
			DrawPrimitives::setPointSize(4);
			DrawPrimitives::setDrawColor4B(0, 0, 255, 255); // Root bone is blue.
			for (int i = 0, n = _skeleton->bonesCount; i < n; i++) {
				spBone *bone = _skeleton->bones[i];
				DrawPrimitives::drawPoint(Vec2(bone->worldX, bone->worldY));
				if (i == 0) DrawPrimitives::setDrawColor4B(0, 255, 0, 255);
			}
		}
		director->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
	}
}

const char * SkeletonRenderer::getCharacterName(spRegionAttachment* attachment) const
{
    return ((spAtlasRegion*)attachment->rendererObject)->name;
}
    
    
Texture2D* SkeletonRenderer::getTexture (spRegionAttachment* attachment) const {
    
    return (Texture2D*)((spAtlasRegion*)attachment->rendererObject)->page->rendererObject;
}

Texture2D* SkeletonRenderer::getTexture (spMeshAttachment* attachment) const {
	return (Texture2D*)((spAtlasRegion*)attachment->rendererObject)->page->rendererObject;
}

Texture2D* SkeletonRenderer::getTexture (spSkinnedMeshAttachment* attachment) const {
	return (Texture2D*)((spAtlasRegion*)attachment->rendererObject)->page->rendererObject;
}

Rect SkeletonRenderer::getBoundingBox () const {
	float minX = FLT_MAX, minY = FLT_MAX, maxX = FLT_MIN, maxY = FLT_MIN;
	float scaleX = getScaleX(), scaleY = getScaleY();
	for (int i = 0; i < _skeleton->slotsCount; ++i) {
		spSlot* slot = _skeleton->slots[i];
		if (!slot->attachment) continue;
		int verticesCount;
		if (slot->attachment->type == SP_ATTACHMENT_REGION) {
			spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
			spRegionAttachment_computeWorldVertices(attachment, slot->bone, _worldVertices);
			verticesCount = 8;
		} else if (slot->attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment* mesh = (spMeshAttachment*)slot->attachment;
			spMeshAttachment_computeWorldVertices(mesh, slot, _worldVertices);
			verticesCount = mesh->verticesCount;
		} else if (slot->attachment->type == SP_ATTACHMENT_SKINNED_MESH) {
			spSkinnedMeshAttachment* mesh = (spSkinnedMeshAttachment*)slot->attachment;
			spSkinnedMeshAttachment_computeWorldVertices(mesh, slot, _worldVertices);
			verticesCount = mesh->uvsCount;
		} else
			continue;
		for (int ii = 0; ii < verticesCount; ii += 2) {
			float x = _worldVertices[ii] * scaleX, y = _worldVertices[ii + 1] * scaleY;
			minX = min(minX, x);
			minY = min(minY, y);
			maxX = max(maxX, x);
			maxY = max(maxY, y);
		}
	}
	Vec2 position = getPosition();
	return Rect(position.x + minX, position.y + minY, maxX - minX, maxY - minY);
}

// --- Convenience methods for Skeleton_* functions.

void SkeletonRenderer::updateWorldTransform () {
	spSkeleton_updateWorldTransform(_skeleton);
}

void SkeletonRenderer::setToSetupPose () {
	spSkeleton_setToSetupPose(_skeleton);
}
void SkeletonRenderer::setBonesToSetupPose () {
	spSkeleton_setBonesToSetupPose(_skeleton);
}
void SkeletonRenderer::setSlotsToSetupPose () {
	spSkeleton_setSlotsToSetupPose(_skeleton);
}

spBone* SkeletonRenderer::findBone (const std::string& boneName) const {
	return spSkeleton_findBone(_skeleton, boneName.c_str());
}

spSlot* SkeletonRenderer::findSlot (const std::string& slotName) const {
	return spSkeleton_findSlot(_skeleton, slotName.c_str());
}

bool SkeletonRenderer::setSkin (const std::string& skinName) {
	return spSkeleton_setSkinByName(_skeleton, skinName.empty() ? 0 : skinName.c_str()) ? true : false;
}
bool SkeletonRenderer::setSkin (const char* skinName) {
	return spSkeleton_setSkinByName(_skeleton, skinName) ? true : false;
}
    
spAttachment* SkeletonRenderer::getAttachment(const std::string& attachmentName)
{
    int i;
    for (i = 0; i < _skeleton->slotsCount; ++i) {
		spAttachment* attachment = spSkeleton_getAttachmentForSlotIndex(_skeleton, i, attachmentName.c_str());
        if(attachment != NULL)
        {
            if(attachment->type == SP_ATTACHMENT_REGION)
            {
                return attachment;
            }
        }
        
	}
    return 0;
}
    
    
    
spRegionAttachment* SkeletonRenderer::getAttachmentOffset(const std::string& attachmentName)
{
    //TO DO get slot name from getcharactername and avoid looping thorugh all slots.
    int i;
    for (i = 0; i < _skeleton->slotsCount; ++i) {
		spAttachment* attachment = spSkeleton_getAttachmentForSlotIndex(_skeleton, i, attachmentName.c_str());
        if(attachment != NULL)
        {
            if(attachment->type == SP_ATTACHMENT_REGION)
            {
                spRegionAttachment * region = SUB_CAST(spRegionAttachment, attachment);
                return region;
              
            }
        }
	}
    return 0;
}

spAttachment* SkeletonRenderer::getAttachment (const std::string& slotName, const std::string& attachmentName) const {
	return spSkeleton_getAttachmentForSlotName(_skeleton, slotName.c_str(), attachmentName.c_str());
}
bool SkeletonRenderer::setAttachment (const std::string& slotName, const std::string& attachmentName) {
	return spSkeleton_setAttachment(_skeleton, slotName.c_str(), attachmentName.empty() ? 0 : attachmentName.c_str()) ? true : false;
}
bool SkeletonRenderer::setAttachment (const std::string& slotName, const char* attachmentName) {
	return spSkeleton_setAttachment(_skeleton, slotName.c_str(), attachmentName) ? true : false;
}

spSkeleton* SkeletonRenderer::getSkeleton () {
	return _skeleton;
}

void SkeletonRenderer::setTimeScale (float scale) {
	_timeScale = scale;
}
float SkeletonRenderer::getTimeScale () const {
	return _timeScale;
}

void SkeletonRenderer::setDebugSlotsEnabled (bool enabled) {
	_debugSlots = enabled;
}
bool SkeletonRenderer::getDebugSlotsEnabled () const {
	return _debugSlots;
}

void SkeletonRenderer::setDebugBonesEnabled (bool enabled) {
	_debugBones = enabled;
}
bool SkeletonRenderer::getDebugBonesEnabled () const {
	return _debugBones;
}

void SkeletonRenderer::onEnter () {
	Node::onEnter();
	scheduleUpdate();
}

void SkeletonRenderer::onExit () {
	Node::onExit();
	unscheduleUpdate();
}

// --- CCBlendProtocol

const BlendFunc& SkeletonRenderer::getBlendFunc () const {
	return _blendFunc;
}

void SkeletonRenderer::setBlendFunc (const BlendFunc &blendFunc) {
	_blendFunc = blendFunc;
}

void SkeletonRenderer::setOpacityModifyRGB (bool value) {
	_premultipliedAlpha = value;
}

bool SkeletonRenderer::isOpacityModifyRGB () const {
	return _premultipliedAlpha;
}

}
