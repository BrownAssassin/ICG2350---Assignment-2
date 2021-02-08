#include "PostEffect.h"

void PostEffect::Init(unsigned width, unsigned height)
{
	int index = int(m_buffers.size());
	m_buffers.push_back(new Framebuffer());
	m_buffers[index]->AddColorTarget(GL_RGBA8);
	m_buffers[index]->AddDepthTarget();
	m_buffers[index]->Init(width, height);

	index = int(m_shaders.size());
	m_shaders.push_back(Shader::Create());
	m_shaders[index]->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
	m_shaders[index]->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
	m_shaders[index]->Link();
}

void PostEffect::ApplyEffect(PostEffect* previousBuffer)
{
	BindShader(0);

	previousBuffer->BindColorAsTexture(0, 0, 0);

	m_buffers[0]->RenderToFSQ();

	previousBuffer->UnbindTexture(0);

	UnbindShader();
}

void PostEffect::DrawToScreen()
{
	BindShader(0);

	BindColorAsTexture(0, 0, 0);

	m_buffers[0]->DrawFullscreenQuad();

	UnbindTexture(0);

	UnbindShader();
}

void PostEffect::Reshape(unsigned width, unsigned height)
{
	for (int i = 0; i < m_buffers.size(); i++)
		m_buffers[i]->Reshape(width, height);
}

void PostEffect::Clear()
{
	for (int i = 0; i < m_buffers.size(); i++)
		m_buffers[i]->Clear();
}

void PostEffect::Unload()
{
	for (int i = 0; i < m_buffers.size(); i++)
	{
		if (m_buffers[i] != nullptr) {
			m_buffers[i]->Unload();
			delete m_buffers[i];
			m_buffers[i] = nullptr;
		}
	}

	m_shaders.clear();
}

void PostEffect::BindBuffer(int index)
{
	m_buffers[index]->Bind();
}

void PostEffect::UnbindBuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

void PostEffect::BindColorAsTexture(int index, int colorBuffer, int textureSlot)
{
	m_buffers[index]->BindColorAsTexture(colorBuffer, textureSlot);
}

void PostEffect::BindDepthAsTexture(int index, int textureSlot)
{
	m_buffers[index]->BindDepthAsTexture(textureSlot);
}

void PostEffect::UnbindTexture(int textureSlot)
{
	glActiveTexture(GL_TEXTURE0 + textureSlot);
	glBindTexture(GL_TEXTURE_2D, GL_NONE);
}

void PostEffect::BindShader(int index)
{
	m_shaders[index]->Bind();
}

void PostEffect::UnbindShader()
{
	glUseProgram(GL_NONE);
}
