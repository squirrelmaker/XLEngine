#include "CommandBuffer.h"
#include "IDriver3D.h"
#include "VertexBuffer.h"
#include <memory.h>
#include <cstdio>
#include <cstdlib>

uint8_t *CommandBuffer::s_pCommandBuffer[ CommandBuffer::BUFFER_COUNT ];
uint32_t CommandBuffer::s_uReadBuffer;
uint32_t CommandBuffer::s_uWriteBuffer;
uint32_t CommandBuffer::s_uCommandPos;
IDriver3D *CommandBuffer::s_pDriver;

//Initialize memory.
void CommandBuffer::Init(IDriver3D *pDriver)
{
    s_pDriver      = pDriver;
    s_uReadBuffer  = 0;
    s_uWriteBuffer = 1;
    s_uCommandPos  = 0;

    for (int c=0; c<CommandBuffer::BUFFER_COUNT; c++)
    {
        s_pCommandBuffer[c] = (uint8_t *)malloc(BUFFER_SIZE);
    }
}

void CommandBuffer::Destroy()
{
    for (int c=0; c<CommandBuffer::BUFFER_COUNT; c++)
    {
        delete s_pCommandBuffer[c];
        s_pCommandBuffer[c] = nullptr;
    }
}

//Commands
void CommandBuffer::SetBlendMode(uint32_t uMode)
{
    CB_BlendMode *pBlendMode = (CB_BlendMode *)CB_Command( sizeof(CB_BlendMode) );
    if ( pBlendMode )
    {
        pBlendMode->type = CB_BLEND_MODE;
        pBlendMode->size = sizeof(CB_BlendMode);
        pBlendMode->blendMode = uMode;
    }
}

void CommandBuffer::SetTexture(int32_t slot, TextureHandle hTex, uint32_t uFilter, bool bWrap)
{
    CB_SetTexture *pSetTexture = (CB_SetTexture *)CB_Command( sizeof(CB_SetTexture) );
    if ( pSetTexture )
    {
        pSetTexture->type = CB_SET_TEXTURE;
        pSetTexture->size = sizeof(CB_SetTexture);
        pSetTexture->hTex = hTex;
        pSetTexture->uFilter = uFilter;
        pSetTexture->bWrap = bWrap;
    }
}

void CommandBuffer::SetVertexBuffer(VertexBuffer *pVB)
{
    CB_SetVertexBuffer *pSetVertexBuffer = (CB_SetVertexBuffer *)CB_Command( sizeof(CB_SetVertexBuffer) );
    if ( pSetVertexBuffer )
    {
        pSetVertexBuffer->type = CB_SET_VERTEXBUFFER;
        pSetVertexBuffer->size = sizeof(CB_SetVertexBuffer);
        pSetVertexBuffer->pVB  = pVB;
    }
}

void CommandBuffer::DrawIndexed(IndexBuffer *pIB, int32_t nStartIndex, int32_t nTriCnt)
{
    CB_DrawIndexed *pDrawIndexed = (CB_DrawIndexed *)CB_Command( sizeof(CB_DrawIndexed) );
    if ( pDrawIndexed )
    {
        pDrawIndexed->type = CB_DRAW_INDEXED;
        pDrawIndexed->size = sizeof(CB_DrawIndexed);
        pDrawIndexed->pIB  = pIB;
        pDrawIndexed->startIndex = nStartIndex;
        pDrawIndexed->primCount  = nTriCnt;
    }
}

void CommandBuffer::DrawCall(const Matrix& mWorld, TextureHandle hTex, VertexBuffer *pVB, IndexBuffer *pIB, int32_t nStartIndex, int32_t nTriCnt)
{
    CB_DrawCall *pDrawCall = (CB_DrawCall *)CB_Command( sizeof(CB_DrawCall) );
    if ( pDrawCall )
    {
        pDrawCall->type = CB_DRAW_CALL;
        pDrawCall->size = sizeof(CB_DrawCall);
        pDrawCall->mWorld = mWorld;
        pDrawCall->hTex = hTex;
        pDrawCall->pVB  = pVB;
        pDrawCall->pIB  = pIB;
        pDrawCall->nStartIndex = nStartIndex;
        pDrawCall->nTriCnt = nTriCnt;
    }
}

//Custom command, returns memory allocated from the command buffer.
void *CommandBuffer::CB_Command(uint32_t uSize)
{
    void *pMem = nullptr;
    if ( s_uCommandPos+uSize+END_MARKER_SIZE <= BUFFER_SIZE )
    {
        pMem = &s_pCommandBuffer[s_uWriteBuffer];
        s_uCommandPos += uSize;
    }
    return pMem;
}

//Execution.
void CommandBuffer::Execute()
{
    //Add the end marker.
    uint16_t *pEndMarker = (uint16_t *)CB_Command( END_MARKER_SIZE );
    pEndMarker[0] = CB_END_MARKER;
    pEndMarker[1] = END_MARKER_SIZE;

    //Swap Buffers.
    s_uReadBuffer  = s_uWriteBuffer;
    s_uWriteBuffer = (s_uWriteBuffer+1)%BUFFER_COUNT;
    s_uCommandPos  = 0;

    //This should be on the render thread.

    //Read the commands from the Read Buffer.
    uint8_t *pReadBuffer = (uint8_t *)s_pCommandBuffer[s_uReadBuffer];
    bool bEndMarkerReached = false;
    while (bEndMarkerReached == false)
    {
        CB_Base *header = (CB_Base *)pReadBuffer;
        switch (header->type)
        {
            case CB_BLEND_MODE:
            {
                CB_BlendMode *pBlendMode = (CB_BlendMode *)pReadBuffer;
                s_pDriver->SetBlendMode( pBlendMode->blendMode );
            }
            break;
            case CB_SET_TEXTURE:
            {
                CB_SetTexture *pSetTexture = (CB_SetTexture *)pReadBuffer;
                s_pDriver->SetTexture( pSetTexture->slot, pSetTexture->hTex, pSetTexture->uFilter, pSetTexture->bWrap );
            }
            break;
            case CB_SET_VERTEXBUFFER:
            {
                CB_SetVertexBuffer *pSetVertexBuffer = (CB_SetVertexBuffer *)pReadBuffer;
                pSetVertexBuffer->pVB->Set();
            }
            break;
            case CB_DRAW_INDEXED:
            {
                CB_DrawIndexed *pDrawIndexed = (CB_DrawIndexed *)pReadBuffer;
                s_pDriver->RenderIndexedTriangles( pDrawIndexed->pIB, pDrawIndexed->primCount );
            }
            break;
            case CB_DRAW_CALL:
            {
                CB_DrawCall *pDrawCall = (CB_DrawCall *)pReadBuffer;
                s_pDriver->SetTexture(0, pDrawCall->hTex);
                pDrawCall->pVB->Set();
                s_pDriver->SetWorldMatrix( &pDrawCall->mWorld, 0, 0 );
                s_pDriver->RenderIndexedTriangles( pDrawCall->pIB, pDrawCall->nTriCnt );
            }
            break;
            case CB_END_MARKER:
            {
                bEndMarkerReached = true;
            }
            break;
        };

        pReadBuffer += header->size;
    };
}
