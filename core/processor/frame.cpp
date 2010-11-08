#include "../StdAfx.h"

#include "frame.h"
#include "../video/pixel_format.h"
#include "../../common/utility/memory.h"
#include "../../common/gl/utility.h"
#include "../../common/gl/pixel_buffer_object.h"

#include <boost/range/algorithm.hpp>

namespace caspar { namespace core {
	
struct rectangle
{
	rectangle(double left, double top, double right, double bottom)
		: left(left), top(top), right(right), bottom(bottom)
	{}
	double left;
	double top;
	double right;
	double bottom;
};

GLubyte progressive_pattern[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xFF, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	
GLubyte upper_pattern[] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00};
		
GLubyte lower_pattern[] = {
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff};
																																						
struct frame::implementation : boost::noncopyable
{
	implementation(size_t width, size_t height)
		: alpha_(1.0f), x_(0.0f), y_(0.0f), update_fmt_(video_update_format::progressive), texcoords_(0.0, 1.0, 1.0, 0.0), pixel_data_(4, nullptr)
	{	
		desc_.planes[0] = pixel_format_desc::plane(width, height, 4);
		desc_.pix_fmt = pixel_format::bgra;
		pixel_data_.resize(4, 0);
		if(width >= 2 && height >= 2)
		{
			pbo_.push_back(std::make_shared<common::gl::pixel_buffer_object>(width, height, GL_BGRA));
			end_write();
		}
	}

	implementation(const pixel_format_desc& desc)
		: alpha_(1.0f), x_(0.0f), y_(0.0f), update_fmt_(video_update_format::progressive), texcoords_(0.0, 1.0, 1.0, 0.0), pixel_data_(4, nullptr)
	{			
		desc_ = desc;

		for(size_t n = 0; n < desc_.planes.size(); ++n)
		{
			if(desc_.planes[n].size == 0)
				break;

			GLuint format = [&]() -> GLuint
			{
				switch(desc_.planes[n].channels)
				{
				case 1: return GL_LUMINANCE;
				case 2: return GL_LUMINANCE_ALPHA;
				case 3: return GL_BGR;
				case 4: return GL_BGRA;
				default: BOOST_THROW_EXCEPTION(out_of_range() << msg_info("1-4 channels are supported") << arg_name_info("desc.planes.channels")); 
				}
			}();

			pbo_.push_back(std::make_shared<common::gl::pixel_buffer_object>(desc_.planes[n].width, desc_.planes[n].height, format));
		}
		end_write();
	}
	
	void begin_write()
	{
		pixel_data_ = std::vector<void*>(4, 0);
		boost::range::for_each(pbo_, std::mem_fn(&common::gl::pixel_buffer_object::begin_write));
	}

	void end_write()
	{
		boost::range::transform(pbo_, pixel_data_.begin(), std::mem_fn(&common::gl::pixel_buffer_object::end_write));
	}
	
	void begin_read()
	{	
		pixel_data_ = std::vector<void*>(4, 0);
		boost::range::for_each(pbo_, std::mem_fn(&common::gl::pixel_buffer_object::begin_read));
	}

	void end_read()
	{
		boost::range::transform(pbo_, pixel_data_.begin(), std::mem_fn(&common::gl::pixel_buffer_object::end_read));
	}

	void draw(const frame_shader_ptr& shader)
	{
		shader->use(desc_);
		glPushMatrix();
		glTranslated(x_*2.0, y_*2.0, 0.0);
		glColor4d(1.0, 1.0, 1.0, alpha_);

		if(update_fmt_ == video_update_format::progressive)
			glPolygonStipple(progressive_pattern);
		else if(update_fmt_ == video_update_format::upper)
			glPolygonStipple(upper_pattern);
		else if(update_fmt_ == video_update_format::lower)
			glPolygonStipple(lower_pattern);

		for(size_t n = 0; n < pbo_.size(); ++n)
		{
			glActiveTexture(GL_TEXTURE0+n);
			pbo_[n]->bind_texture();
		}
		glBegin(GL_QUADS);
			glTexCoord2d(texcoords_.left,	texcoords_.bottom); glVertex2d(-1.0, -1.0);
			glTexCoord2d(texcoords_.right,	texcoords_.bottom); glVertex2d( 1.0, -1.0);
			glTexCoord2d(texcoords_.right,	texcoords_.top);	glVertex2d( 1.0,  1.0);
			glTexCoord2d(texcoords_.left,	texcoords_.top);	glVertex2d(-1.0,  1.0);
		glEnd();
		glPopMatrix();
	}

	unsigned char* data(size_t index)
	{
		if(pbo_.size() < index)
			BOOST_THROW_EXCEPTION(out_of_range());
		return static_cast<unsigned char*>(pixel_data_[index]);
	}

	void reset()
	{
		audio_data_.clear();
		alpha_		= 1.0f;
		x_			= 0.0f;
		y_			= 0.0f;
		texcoords_	= rectangle(0.0, 1.0, 1.0, 0.0);
		update_fmt_ = video_update_format::progressive;
	}

	std::vector<common::gl::pixel_buffer_object_ptr> pbo_;
	std::vector<void*> pixel_data_;	
	std::vector<short> audio_data_;

	double alpha_;
	double x_;
	double y_;
	video_update_format::type update_fmt_;
	rectangle texcoords_;

	pixel_format_desc desc_;
};

frame::frame(size_t width, size_t height) 
	: impl_(new implementation(width, height)){}
frame::frame(const pixel_format_desc& desc)
	: impl_(new implementation(desc)){}
void frame::draw(const frame_shader_ptr& shader){impl_->draw(shader);}
void frame::begin_write(){impl_->begin_write();}
void frame::end_write(){impl_->end_write();}	
void frame::begin_read(){impl_->begin_read();}
void frame::end_read(){impl_->end_read();}
void frame::pix_fmt(pixel_format::type format) {impl_->desc_.pix_fmt = format;}
unsigned char* frame::data(size_t index){return impl_->data(index);}
size_t frame::size(size_t index) const { return impl_->desc_.planes[index].size; }
std::vector<short>& frame::audio_data() { return impl_->audio_data_; }
void frame::reset(){impl_->reset();}
void frame::alpha(double value){ impl_->alpha_ = value;}
void frame::translate(double x, double y) { impl_->x_ += x; impl_->y_ += y; }
void frame::texcoords(double left, double top, double right, double bottom){impl_->texcoords_ = rectangle(left, top, right, bottom);}
void frame::update_fmt(video_update_format::type fmt){ impl_->update_fmt_ = fmt;}
double frame::x() const { return impl_->x_;}
double frame::y() const { return impl_->y_;}
}}