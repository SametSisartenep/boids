#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

enum {
	TERMINALV = 20
};

enum {
	Cbg,
	Cfg,
	NCOLOR
};

typedef struct Bird Bird;
typedef struct Flock Flock;

struct Bird
{
	Point2 p, v;
	Image *color;
};

struct Flock
{
	Bird *birds;
	int nbirds;
	Rectangle jail;

	void (*separate)(Flock*);
	void (*align)(Flock*);
	void (*cohesion)(Flock*);
	void (*step)(Flock*);
};

RFrame worldrf;
Image *pal[NCOLOR];
Flock *flock;

void flockseparate(Flock*);
void flockalign(Flock*);
void flockcohesion(Flock*);
void flockstep(Flock*);

Flock*
newflock(int nbirds, Rectangle jail)
{
	Flock *f;
	Bird *b;

	f = malloc(sizeof(Flock));
	if(f == nil)
		sysfatal("malloc: %r");
	setmalloctag(f, getcallerpc(&nbirds));
	f->birds = nil;
	f->nbirds = 0;
	f->jail = jail;
	f->separate = flockseparate;
	f->align = flockalign;
	f->cohesion = flockcohesion;
	f->step = flockstep;

	while(nbirds--){
		f->birds = realloc(f->birds, ++f->nbirds*sizeof(Bird));
		if(f->birds == nil)
			sysfatal("realloc: %r");
		setrealloctag(f->birds, getcallerpc(&nbirds));
		b = &f->birds[f->nbirds-1];
		b->p = Pt2(frand()*Dx(jail),frand()*Dy(jail),1);
		b->v = Vec2(cos(frand()*2*PI),sin(frand()*2*PI));
		b->v = mulpt2(b->v, TERMINALV);
		b->color = pal[Cfg];
	}

	return f;
}

void
flockseparate(Flock *)
{
}

void
flockalign(Flock *)
{
}

void
flockcohesion(Flock *)
{
}

void
flockstep(Flock *f)
{
	static double Δt = 1;
	Bird *b;

	//f->separate(f);
	//f->align(f);
	//f->cohesion(f);
	for(b = f->birds; b < f->birds + f->nbirds; b++){
		b->p = addpt2(b->p, mulpt2(b->v, Δt));
		if(b->p.x < 0){
			b->p.x = 0;
			b->v.x = -b->v.x;
		}
		if(b->p.y < 0){
			b->p.y = 0;
			b->v.y = -b->v.y;
		}
		if(b->p.x > Dx(f->jail)){
			b->p.x = Dx(f->jail);
			b->v.x = -b->v.x;
		}
		if(b->p.y > Dy(f->jail)){
			b->p.y = Dy(f->jail);
			b->v.y = -b->v.y;
		}
	}
}

Point
toscreen(Point2 p)
{
	p = invrframexform(p, worldrf);
	return Pt(p.x,p.y);
}

Point2
fromscreen(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), worldrf);
}

void
initpalette(void)
{
	pal[Cbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000ff);
	pal[Cfg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xffffffff);
}

void
redraw(void)
{
	Bird *b;

	lockdisplay(display);
	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(b = flock->birds; b < flock->birds + flock->nbirds; b++){
		ellipse(screen, toscreen(b->p), 2, 2, 0, b->color, ZP);
	}
	flushimage(display, 1);
	unlockdisplay(display);
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	flock->jail = Rect(0,0,Dx(screen->r),Dy(screen->r));
	redraw();
}

void
mouse(Mouse)
{
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	case ' ':
		flock->step(flock);
		break;
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-n nbirds]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;
	char *s;
	int nbirds;

	nbirds = 10;
	ARGBEGIN{
	case 'n':
		nbirds = strtol(EARGF(usage()), &s, 10);
		if(*s != 0)
			usage();
		break;
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();

	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	initpalette();
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	worldrf.bx = Vec2(1, 0);
	worldrf.by = Vec2(0,-1);

	flock = newflock(nbirds, Rect(0,0,Dx(screen->r),Dy(screen->r)));

	display->locking = 1;
	unlockdisplay(display);
	redraw();

	for(;;){
		enum { MOUSE, RESIZE, KEYBOARD };
		Alt a[] = {
			{mc->c, &mc->Mouse, CHANRCV},
			{mc->resizec, nil, CHANRCV},
			{kc->c, &r, CHANRCV},
			{nil, nil, CHANEND}
		};

		switch(alt(a)){
		case MOUSE:
			mouse(mc->Mouse);
			break;
		case RESIZE:
			resized();
			break;
		case KEYBOARD:
			key(r);
			break;
		}

		redraw();
	}
}
