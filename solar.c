#define OUTPUTSECJUMP 0.01666666666 // 60 hz
#define OUTPUTLIMIT imgcount < 1800
#define NUMPLANETS 20


//#define USEOUTPUT
//#define POSTPROCESS
//#define USEFRAMEBUFFER

#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <time.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION


#ifdef USEOUTPUT
#include "stb_image_write.h"
#endif

#ifdef USEFRAMEBUFFER
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#endif

#define TRUE 1
#define FALSE 0


const char * circleshadervert = "\
	attribute vec2 tcattrib;\n\
	attribute vec3 posattrib;\n\
	attribute vec3 colattrib;\n\
	varying vec3 col;\n\
	varying vec2 tc;\n\
	uniform mat4 proj;\n\
	void main(){\n\
		col = colattrib * 1.0;\n\
		tc = tcattrib;\n\
		gl_Position = proj * vec4(posattrib, 1.0);\n\
	}\n\
";
const char * circleshaderfrag = "\
	varying vec2 tc;\n\
	varying vec3 col;\n\
	void main(){\n\
		float dist = length(tc);\n\
		if(dist >=1.0) discard;\n\
		vec2 dister = vec2(dFdx(dist), dFdy(dist));\n\
		float crazy = length(dister);\n\
		dist=abs(1.0-crazy-dist)/crazy;\n\
		float res = max(0.0, mix(1.0, 0.0, dist));\n\
//		if(res <=0.0) discard;\n\
		gl_FragColor.rgba = vec4(col*res, 1.0);\n\
	}\n\
";
const char * lineshadervert = "\
	attribute vec3 posattrib;\n\
	uniform mat4 proj;\n\
	void main(){\n\
		gl_Position = proj * vec4(posattrib, 1.0);\n\
	}\n\
";
const char * lineshaderfrag = "\
	uniform vec3 col;\n\
	void main(){\n\
		gl_FragColor.rgba = vec4(col, 1.0);\n\
	}\n\
";
//postprocessing shaders
const char * fsshadervert ="\
	attribute vec2 posattrib;\n\
	varying vec2 screencoord;\n\
	void main(){\n\
		gl_Position = vec4(posattrib, 0.0, 1.0);\n\
		screencoord = (posattrib + vec2(1.0)) * 0.5;\n\
	}\n\
";
	// dunno where i got the rand2s... probably iq. afl pasted it in irc
const char * fsblurshaderfrag = "\
	#version 130\n\
	uniform sampler2D texture0;\n\
	varying vec2 screencoord;\n\
	float rand2s(vec2 co){\n\
		return fract(sin(dot(co.xy,vec2(12.9898,78.233))) * 43758.5453);\n\
	}\n\
	void main(){\n\
//#define EDGEDETECT\n\
#ifdef EDGEDETECT\n\
		vec3 point;\n\
		float len = 0.0;\n\
		vec3 initial = texture(texture0, screencoord).rgb;\n\
		point = textureOffset(texture0, screencoord, ivec2(-1.0, 0.0)).rgb;\n\
		len += step(0.5, length(initial-point));\n\
		point = textureOffset(texture0, screencoord, ivec2(1.0, 0.0)).rgb;\n\
		len += step(0.5, length(initial-point));\n\
		point = textureOffset(texture0, screencoord, ivec2(0.0, -1.0)).rgb;\n\
		len += step(0.5, length(initial-point));\n\
		point = textureOffset(texture0, screencoord, ivec2(0.0, 1.0)).rgb;\n\
		len += step(0.5, length(initial-point));\n\
\
//		point = textureOffset(texture0, screencoord, ivec2(-1.0, 1.0)).rgb;\n\
//		len += step(0.5, length(initial-point));\n\
//		point = textureOffset(texture0, screencoord, ivec2(1.0, 1.0)).rgb;\n\
//		len += step(0.5, length(initial-point));\n\
//		point = textureOffset(texture0, screencoord, ivec2(-1.0, -1.0)).rgb;\n\
//		len += step(0.5, length(initial-point));\n\
//		point = textureOffset(texture0, screencoord, ivec2(1.0, -1.0)).rgb;\n\
//		len += step(0.5, length(initial-point));\n\
		gl_FragColor.rgb = vec3(len * 0.1 * initial);\n\
#else\n\
		vec3 gather = textureLod(texture0, screencoord, 1.0 + rand2s(screencoord * 1.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 2.0 + rand2s(screencoord * 2.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 3.0 + rand2s(screencoord * 3.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 4.0 + rand2s(screencoord * 4.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 5.0 + rand2s(screencoord * 5.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 6.0 + rand2s(screencoord * 6.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 7.0 + rand2s(screencoord * 7.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 8.0 + rand2s(screencoord * 8.0)).rgb;\n\
		gather += textureLod(texture0, screencoord, 9.0 + rand2s(screencoord * 9.0)).rgb;\n\
		gather = texture(texture0, screencoord).rgb + gather* 0.2;\n\
		gl_FragColor.rgba = vec4(gather, 1.0);\n\
#endif\n\
	}\n\
";


int shader_printProgramLogStatus(const int id){
	GLint blen = 0;
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &blen);
	if(blen > 1){
		GLchar * log = (GLchar *) malloc(blen);
		glGetProgramInfoLog(id, blen, 0, log);
		printf("program log: \n%s\n", log);
		free(log);
	}
	return blen;
}
int shader_printShaderLogStatus(const int id){
	GLint blen = 0;
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &blen);
	if(blen > 1){
		GLchar * log = (GLchar *) malloc(blen);
		glGetShaderInfoLog(id, blen, 0, log);
		printf("shader log: \n%s\n", log);
		free(log);
	}
	return blen;
}

#define COLATTRIBLOC 2
#define POSATTRIBLOC 0
#define TCATTRIBLOC 1

int width = 1280;
int height = 720;

typedef struct planet_s {
	float color[3];
	float period;
	float size;
	float dist;
	int parent; //todo
	int numtrail;
	int sizetrail;
	int postrail;
	GLfloat * verts;	//pre-offset positions of line verts
	GLfloat * ofs;		//unit vector offset in 3d?
}planet_t;

GLuint trailindices = 0;

int genIndices(int num){
	//figure out the number of line segements
	int seg = num-1;
	if(seg < 1) return 0;
	//4 triangles per segment, except for the last segment, which is only 2
	int tris = 4 * seg -2;
	if(trailindices) free(trailindices();
	trailindices = malloc(tris * 3 * sizeof (GLuint));

	//generate maintrails
	int i;
	for(i = 0; i < seg; i++){
		tris[i*12 + 0] = seg;
		tris[i*12 + 1] = seg+1;
		tris[i*12 + 2] = seg+2;

		tris[i*12 + 3] = seg+3;
		tris[i*12 + 4] = seg+2;
		tris[i*12 + 5] = seg+1;
	}
	//generate conbars
	for(i = 1; i < seg; i++){
		tris[i*12 + 5] = seg+3;
		tris[i*12 + 6] = seg+4;
		tris[i*12 + 7] = seg+2;
	}
}

planet_t *planets;

size_t numplanets;
void genPlanets(size_t np){
	numplanets = np;
	planets = malloc(np * sizeof(planet_t));
	planet_t *p = planets;
	p->size = 0.1;
	p->dist = 0.0;
	p->period = 0.0;
	p->color[0] = 1.0;
	p->color[1] = 1.0;
	p->color[2] = 0.0;
	p->sizetrail = 512;
	p->verts = malloc(p->sizetrail * 3 *sizeof(GLfloat));

	unsigned int i;
	for(i = 1; i < np; i++){
		planet_t *p = &planets[i];
		p->size = (double)rand()/(double)RAND_MAX * 0.1;
		p->dist = (double)rand()/(double)RAND_MAX * 2.0;

		//check possible collisions with earlier planets
		int j;
		for(j = 0; j < i; j++) if(p->dist + p->size > planets[j].dist - planets[j].size && p->dist - p->size < planets[j].dist + planets[j].size) break;
		if(i != j){i--; /*printf("Skipped\n"); */continue;}
		p->period = 5.0/p->dist;
		if(!(rand() % 10)) p->period*= -1.0;
		p->color[0] = (double)rand()/(double)RAND_MAX;
		p->color[1] = (double)rand()/(double)RAND_MAX;
		p->color[2] = (double)rand()/(double)RAND_MAX;
		p->sizetrail = 512;
		p->verts = malloc(p->sizetrail * 6 *sizeof(GLfloat));
		p->verts = malloc(p->sizetrail * 4 *sizeof(GLfloat));
	}

}

#ifdef USEFRAMEBUFFER
int fb_fd = 0;
struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
unsigned char *fbp;
int fbwidth =-1;
int fbheight = -1;
void initfb(void){
	fb_fd = open("/dev/fb0", O_RDWR);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale = 0;
	vinfo.bits_per_pixel = 32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
//	vinfo.xres_virtual = vinfo.xres;
//	vinfo.yres_virtual = vinfo.yres;
//	vinfo.xoffset = 0;
//	vinfo.yoffset = 0;
//	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
//	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

	printf("framebuffer offsets are %i %i %i\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);

	long screensize = vinfo.yres_virtual * finfo.line_length;
	fbwidth = vinfo.xres;
	fbheight = vinfo.yres;

	fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t) 0);
}
#endif
float projmat[16] = {0.0};

GLuint fb1, fb1rb, fb1t;
GLuint shaderid, lineshaderid;
GLFWwindow * window;

int resize(int inwidth, int inheight){
	width = inwidth;
	height = inheight;
//generate projection matrix
	double sine, cotangent, deltaZ;
	double fov = 90.0, aspect = (double)width/(double)height;
	double far = 100.0, near = 0.1;
	double radians = fov / 2.0 * M_PI / 180.0;
	deltaZ = far - near;
	sine = sin(radians);
	cotangent = cos(radians)/sine;
	projmat[0*4+0] = cotangent / aspect;
	projmat[1*4+1] = cotangent;
	projmat[2*4+2] = -(far + near) / deltaZ;
	projmat[2*4+3] = -1.0;
	projmat[3*4+2] = -2.0 * near * far / deltaZ;

#ifdef POSTPROCESS

	glBindRenderbuffer(GL_RENDERBUFFER, fb1rb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);

	glBindTexture(GL_TEXTURE_2D, fb1t);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

#endif

	glUseProgram(shaderid);
	GLuint projloc = glGetUniformLocation(shaderid, "proj");
	glUniformMatrix4fv(projloc, 1, GL_FALSE, projmat);

	glUseProgram(lineshaderid);
	GLuint lprojloc = glGetUniformLocation(lineshaderid, "proj");
	glUniformMatrix4fv(lprojloc, 1, GL_FALSE, projmat);


	glfwSetWindowSize(window, width, height);
	glViewport(0,0, width, height);
	printf("resizzie %i %i\n", width, height);
}

int main(void){
	#ifdef USEFRAMEBUFFER
		initfb();
	#endif
	srand(time(0));
	genPlanets(NUMPLANETS);
	if(!glfwInit()) return -1;
	window 	= glfwCreateWindow(width, height, "solar", NULL, NULL);
	if (!window){
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	GLenum glewError = glewInit();
	if(glewError != GLEW_OK){
		printf( "Error initializing GLEW! %s\n", glewGetErrorString( glewError ) );
		return FALSE;
	}
	//gl and glew are good 2 go
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);



//circle shader
	GLuint vertid = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragid = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vertid, 1, (const GLchar **)&circleshadervert, 0);
	glShaderSource(fragid, 1, (const GLchar **)&circleshaderfrag, 0);
	glCompileShader(vertid);
	shader_printShaderLogStatus(vertid);
	glCompileShader(fragid);
	shader_printShaderLogStatus(fragid);
	shaderid = glCreateProgram();
	if(!shaderid) printf("unable to greate program\n");
	glAttachShader(shaderid, vertid);
	glAttachShader(shaderid, fragid);
	glBindAttribLocation(shaderid, COLATTRIBLOC, "colattrib");
	glBindAttribLocation(shaderid, POSATTRIBLOC, "posattrib");
	glBindAttribLocation(shaderid, TCATTRIBLOC, "tcattrib");
	glLinkProgram(shaderid);
	glDeleteShader(vertid);
	glDeleteShader(fragid);
	shader_printProgramLogStatus(shaderid);
	glUseProgram(shaderid);



//line shader
	vertid = glCreateShader(GL_VERTEX_SHADER);
	fragid = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vertid, 1, (const GLchar **)&lineshadervert, 0);
	glShaderSource(fragid, 1, (const GLchar **)&lineshaderfrag, 0);
	glCompileShader(vertid);
	shader_printShaderLogStatus(vertid);
	glCompileShader(fragid);
	shader_printShaderLogStatus(fragid);
	lineshaderid = glCreateProgram();
	if(!lineshaderid) printf("unable to greate program\n");
	glAttachShader(lineshaderid, vertid);
	glAttachShader(lineshaderid, fragid);
	glBindAttribLocation(lineshaderid, COLATTRIBLOC, "colattrib");
	glBindAttribLocation(lineshaderid, POSATTRIBLOC, "posattrib");
	glBindAttribLocation(lineshaderid, TCATTRIBLOC, "tcattrib");
	glLinkProgram(lineshaderid);
	glDeleteShader(vertid);
	glDeleteShader(fragid);
	shader_printProgramLogStatus(lineshaderid);
	glUseProgram(lineshaderid);
	GLuint colloc = glGetUniformLocation(lineshaderid, "col");
#ifdef POSTPROCESS

//fsblur shader
	vertid = glCreateShader(GL_VERTEX_SHADER);
	fragid = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vertid, 1, (const GLchar **)&fsshadervert, 0);
	glShaderSource(fragid, 1, (const GLchar **)&fsblurshaderfrag, 0);
	glCompileShader(vertid);
	shader_printShaderLogStatus(vertid);
	glCompileShader(fragid);
	shader_printShaderLogStatus(fragid);
	GLuint fsshaderid = glCreateProgram();
	if(!fsshaderid) printf("unable to greate program\n");
	glAttachShader(fsshaderid, vertid);
	glAttachShader(fsshaderid, fragid);
	glBindAttribLocation(fsshaderid, POSATTRIBLOC, "posattrib");
	glLinkProgram(fsshaderid);
	glDeleteShader(vertid);
	glDeleteShader(fragid);
	shader_printProgramLogStatus(fsshaderid);
	glUseProgram(fsshaderid);
	GLuint texloc = glGetUniformLocation(fsshaderid, "texture0");
	glUniform1i(texloc, 0);

	//set up framebuffer
	glGenFramebuffers(1, &fb1);
	glBindFramebuffer(GL_FRAMEBUFFER, fb1);
	glGenRenderbuffers(1, &fb1rb);
	glBindRenderbuffer(GL_RENDERBUFFER, fb1rb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb1rb);
	glGenTextures(1, &fb1t);
	glBindTexture(GL_TEXTURE_2D, fb1t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 10); //todo find this proper
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glGenerateMipmap(GL_TEXTURE_2D);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb1t, 0);



	resize(width, height);

	//swole
	GLfloat fsquad[12] = {-1.0, -1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0,};


#endif

	GLfloat * verts = malloc((numplanets * 12) * sizeof(GLfloat));
	GLfloat * tc = malloc((numplanets * 8) * sizeof(GLfloat));
	GLfloat * col = malloc((numplanets * 12) * sizeof(GLfloat));
	GLuint * indices = malloc((numplanets * 6) * sizeof(GLuint));
//generate planet buffers



	int i;
	for(i = 0; i < numplanets; i++){
		tc[i * 8 +0] = -1.0;
		tc[i * 8 +1] = -1.0;

		tc[i * 8 +2] = 1.0;
		tc[i * 8 +3] = -1.0;

		tc[i * 8 +4] = 1.0;
		tc[i * 8 +5] = 1.0;

		tc[i * 8 +6] = -1.0;
		tc[i * 8 +7] = 1.0;

		indices[i *6 +0] = i * 4 + 0;
		indices[i *6 +1] = i * 4 + 1;
		indices[i *6 +2] = i * 4 + 2;

		indices[i *6 +3] = i * 4 + 0;
		indices[i *6 +4] = i * 4 + 2;
		indices[i *6 +5] = i * 4 + 3;
		int k;
		for(k = 0; k < 4; k++){
			col[i*12 + k*3 + 0] = planets[i].color[0];
			col[i*12 + k*3 + 1] = planets[i].color[1];
			col[i*12 + k*3 + 2] = planets[i].color[2];
		}
	}
	glEnableVertexAttribArray(POSATTRIBLOC);
	glVertexAttribPointer(POSATTRIBLOC, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), verts);
	glEnableVertexAttribArray(COLATTRIBLOC);
	glVertexAttribPointer(COLATTRIBLOC, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), col);
	glEnableVertexAttribArray(TCATTRIBLOC);
	glVertexAttribPointer(TCATTRIBLOC, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), tc);




	double oldsec = glfwGetTime();
	#ifdef USEFRAMEBUFFER
	GLubyte * imagedata = malloc(width * height * 4);
	#endif
	#ifdef USEOUTPUT
	GLubyte * imagedata = malloc(width * height * 3);
	long long unsigned int imgcount = 0;
	for(imgcount = 0; OUTPUTLIMIT; imgcount++){
	#else
	while(!glfwWindowShouldClose(window)){
	#endif

	#ifdef POSTPROCESS
		glBindFramebuffer(GL_FRAMEBUFFER, fb1);
	#endif
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		#ifdef USEOUTPUT
		double seconds = (imgcount * OUTPUTSECJUMP) + oldsec;
		double delta = OUTPUTSECJUMP;
		#else
		double seconds = glfwGetTime(); //yes delta time sue me
		double delta = seconds - oldsec;
		oldsec = seconds;
		#endif
		//do planet physics

		float xoff = 1.0, yoff = 0.0, zoff = -2.0;
		float speedx = -2.0 * delta, speedy = 0.0 * delta, speedz = 0.0 * delta;


		glUseProgram(lineshaderid);
		int i;
		for(i = 0; i < numplanets; i++){
			//figure out x y and z for planet
			planet_t p = planets[i];
			float plx = xoff;
			float ply = yoff + p.dist * sin(seconds * p.period);
			float plz = zoff + p.dist * cos(seconds * p.period);
			//now update their positions
			verts[i*12 +0] = plx - p.size;
			verts[i*12 +1] = ply - p.size;
			verts[i*12 +2] = plz;
			verts[i*12 +3] = plx + p.size;
			verts[i*12 +4] = ply - p.size;
			verts[i*12 +5] = plz;
			verts[i*12 +6] = plx + p.size;
			verts[i*12 +7] = ply + p.size;
			verts[i*12 +8] = plz;
			verts[i*12 +9] = plx - p.size;
			verts[i*12 +10] = ply + p.size;
			verts[i*12 +11] = plz;
			//update trails
			if(p.numtrail < p.sizetrail) planets[i].numtrail++;
			GLfloat *vp;
			for(vp = p.verts + p.numtrail * 3; vp > p.verts; vp-= 3){
				vp[0] = vp[-3] + speedx;
				vp[1] = vp[-2] + speedy;
				vp[2] = vp[-1] + speedz;
			}
			p.verts[0] = plx;
			p.verts[1] = ply;
			p.verts[2] = plz;
			//render trails
			glVertexAttribPointer(POSATTRIBLOC, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), p.verts);
			glUniform3fv(colloc, 1, p.color);
			glDrawArrays(GL_LINE_STRIP, 0, p.numtrail);

		}
		glClear(GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderid);
		glVertexAttribPointer(POSATTRIBLOC, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), verts);
		glDrawElements(GL_TRIANGLES, numplanets * 6, GL_UNSIGNED_INT, indices);

		#ifdef POSTPROCESS
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glGenerateMipmap(GL_TEXTURE_2D);

		glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT);
		glUseProgram(fsshaderid);
		glVertexAttribPointer(POSATTRIBLOC, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), fsquad);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices); //lol abusing indices
		#endif


		#ifdef USEOUTPUT
			glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imagedata);
			char filename[1024];
			filename[0] = 0;
			sprintf(filename, "solarout%llu.tga", imgcount);
			if(stbi_write_tga(filename, width, height, 3, imagedata)){
				printf("Wrote %s\n", filename);
			} else {
				printf("Error writing %s\n", filename);
				return TRUE;
			}
		#endif

		#ifdef USEFRAMEBUFFER
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, imagedata);
//			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, fbp + (vinfo.xoffset * vinfo.bits_per_pixel/8) + vinfo.yoffset * finfo.line_length);
			int line;
			for(line = 0; line < height; line++){
				memcpy(fbp + (vinfo.xoffset * vinfo.bits_per_pixel/8) + (line + vinfo.yoffset) * finfo.line_length, imagedata + line * width * 4, width * 4);
			}
		#endif

		glfwSwapBuffers(window);
		glfwPollEvents();
		if(glfwWindowShouldClose(window)){
			glfwTerminate();
			exit(0);
		}
		int iw, ih;
		glfwGetWindowSize(window, &iw, &ih);
		if(iw != width || ih != height) resize(iw, ih);
	}
}
