#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

enum {
	TERMINALV = 40,
	BORDER = 100
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
	double sight;
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
int nbirds;

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
	f->nbirds = nbirds;
	f->jail = jail;
	f->separate = flockseparate;
	f->align = flockalign;
	f->cohesion = flockcohesion;
	f->step = flockstep;

	f->birds = malloc(nbirds*sizeof(Bird));
	if(f->birds == nil)
		sysfatal("malloc: %r");
	setmalloctag(f->birds, getcallerpc(&nbirds));
	for(b = f->birds; b < f->birds + f->nbirds; b++){
		b->p = Pt2(frand()*jail.max.x + jail.min.x,frand()*jail.max.y + jail.min.y,1);
		b->v = Vec2(cos(frand()*2*PI),sin(frand()*2*PI));
		b->v = mulpt2(b->v, TERMINALV);
		b->sight = 75;
		b->color = pal[Cfg];
	}

	return f;
}

void
flockseparate(Flock *f)
{
	static double comfortdist = 8;
	static double repel = 0.05;
	Point2 repelling, Δp;
	Bird *b0, *b1;

	for(b0 = f->birds; b0 < f->birds + f->nbirds; b0++){
		repelling = Vec2(0,0);

		for(b1 = f->birds; b1 < f->birds + f->nbirds; b1++)
			if(b0 != b1){
				Δp = subpt2(b0->p, b1->p);
				if(vec2len(Δp) < comfortdist)
					repelling = addpt2(repelling, Δp);
			}

		b0->v = addpt2(b0->v, mulpt2(repelling, repel));
	}
}

void
flockalign(Flock *f)
{
	static double attract = 0.05;
	int neighbors;
	Point2 attracting;
	Bird *b0, *b1;

	for(b0 = f->birds; b0 < f->birds + f->nbirds; b0++){
		neighbors = 0;
		attracting = Vec2(0,0);

		for(b1 = f->birds; b1 < f->birds + f->nbirds; b1++)
			if(vec2len(subpt2(b0->p, b1->p)) < b0->sight){
				attracting = addpt2(attracting, b1->v);
				neighbors++;
			}

		if(neighbors > 0){
			attracting = divpt2(attracting, neighbors);
			b0->v = addpt2(b0->v, mulpt2(subpt2(attracting, b0->v), attract));
		}
	}
}

void
flockcohesion(Flock *f)
{
	static double center = 0.005;
	int neighbors;
	Point2 centering;
	Bird *b0, *b1;

	for(b0 = f->birds; b0 < f->birds + f->nbirds; b0++){
		neighbors = 0;
		centering = Vec2(0,0);

		for(b1 = f->birds; b1 < f->birds + f->nbirds; b1++)
			if(vec2len(subpt2(b0->p, b1->p)) < b0->sight){
				centering = addpt2(centering, b1->p);
				neighbors++;
			}

		if(neighbors > 0){
			centering = divpt2(centering, neighbors);
			b0->v = addpt2(b0->v, mulpt2(subpt2(centering, b0->p), center));
		}
	}
}

void
flockstep(Flock *f)
{
	static double Δt = 1;
	static Point2 a = {2,2}; /* deceleration */
	Bird *b;

	f->separate(f);
	f->align(f);
	f->cohesion(f);
	for(b = f->birds; b < f->birds + f->nbirds; b++){
		b->p = addpt2(b->p, mulpt2(b->v, Δt));
		if(b->p.x < f->jail.min.x+BORDER)
			b->v.x += a.x;
		if(b->p.y < f->jail.min.y+BORDER)
			b->v.y += a.y;
		if(b->p.x > f->jail.max.x-BORDER)
			b->v.x -= a.x;
		if(b->p.y > f->jail.max.y-BORDER)
			b->v.y -= a.y;
	}
}

void
resetflock(void)
{
	flock = newflock(nbirds, Rect(0,0,Dx(screen->r),Dy(screen->r)));
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
	for(b = flock->birds; b < flock->birds + flock->nbirds; b++)
		ellipse(screen, toscreen(b->p), 2, 2, 0, b->color, ZP);
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
rmb(Mousectl *mc, Keyboardctl *kc)
{
	enum {
		RESET,
		NBIRDS
	};
	static char *items[] = {
	 [RESET]	"reset",
	 [NBIRDS]	"set bird number",
		nil
	};
	static Menu menu = { .item = items };
	char buf[128];

	switch(menuhit(3, mc, &menu, nil)){
	case NBIRDS:
		snprint(buf, sizeof buf, "%d", nbirds);
		enter("nbirds", buf, sizeof buf, mc, kc, nil);
		nbirds = strtol(buf, nil, 10);
	case RESET:
		srand(time(nil));
		resetflock();
		break;
	}
}

void
mouse(Mousectl *mc, Keyboardctl *kc)
{
	if((mc->buttons&4) != 0)
		rmb(mc, kc);
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

	nbirds = 100;
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
	resetflock();

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
			mouse(mc, kc);
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
