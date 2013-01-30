#include "game/game_main.h"

#define A5I3

extern doorCollection_struct doorCollection;

u32* testDL=NULL;

void initRectangleList(rectangleList_struct* p)
{
	p->first=NULL;
	p->num=0;
}

rectangle_struct* addRectangle(rectangle_struct r, rectangleList_struct* p)
{
	listCell_struct* pc=(listCell_struct*)malloc(sizeof(listCell_struct));
	pc->next=p->first;
	pc->data=r;
	p->first=pc;
	p->num++;
	return &pc->data;
}

void popRectangle(rectangleList_struct* p)
{
	p->num--;
	if(p->first)
	{
		listCell_struct* pc=p->first;
		p->first=pc->next;
		free(pc);
	}
}

void initRectangle(rectangle_struct* rec, vect3D pos, vect3D size)
{
	int x, y;
	s8 sign=1;
	rec->position=pos;
	rec->size=size;
	rec->normal=vect(0,0,0);
	rec->touched=false;
	if(!size.x)
	{
		x=abs(size.y)*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2);
		y=abs(size.z)*LIGHTMAPRESOLUTION;
		rec->normal.x=inttof32(1);
	}else if(!size.y)
	{
		x=abs(size.x)*LIGHTMAPRESOLUTION;
		y=abs(size.z)*LIGHTMAPRESOLUTION;
		rec->normal.y=inttof32(-1);
	}else{
		x=abs(size.x)*LIGHTMAPRESOLUTION;
		y=abs(size.y)*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2);
		rec->normal.z=inttof32(1);
	}
	sign*=(size.x>=0)?1:-1;
	sign*=(size.y>=0)?1:-1;
	sign*=(size.z>=0)?1:-1;
	rec->normal=vect(rec->normal.x*sign,rec->normal.y*sign,rec->normal.z*sign);
	// NOGBA("n : %d %d %d",rec->normal.x,rec->normal.y,rec->normal.z);
	rec->lmSize.x=x;
	rec->lmSize.y=y;
	rec->hide=false;
}

vect3D getUnitVect(rectangle_struct* rec)
{
	vect3D u=vect(0,0,0);
	vect3D size=rec->size;	
	if(size.x>0)u.x=(TILESIZE*2)/LIGHTMAPRESOLUTION;
	else if(size.x)u.x=-(TILESIZE*2)/LIGHTMAPRESOLUTION;
	if(size.y>0)u.y=(TILESIZE*2)/LIGHTMAPRESOLUTION;
	else if(size.y)u.y=-(TILESIZE*2)/LIGHTMAPRESOLUTION;
	if(size.z>0)u.z=(TILESIZE*2)/LIGHTMAPRESOLUTION;
	else if(size.z)u.z=-(TILESIZE*2)/LIGHTMAPRESOLUTION;
	return u;
}

bool collideLineMap(room_struct* r, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i, vect3D* n)
{
	listCell_struct *lc=r->rectangles.first;
	vect3D v;
	while(lc)
	{
		if(&lc->data!=rec)
		{
			// NOGBA("%p vs %p",rec,&lc->data);
			if(collideLineRectangle(&lc->data,l,u,d,NULL,&v)){if(i)*i=v;if(n)*n=lc->data.normal;return true;}
		}
		lc=lc->next;
	}
	return false;
}

rectangle_struct* collideGridCell(gridCell_struct* gc, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i, vect3D* n)
{
	if(!gc)return NULL;
	vect3D v;
	int j;
	for(j=0;j<gc->numRectangles;j++)
	{
		if(gc->rectangles[j]!=rec)
		{
			if(collideLineRectangle(gc->rectangles[j],l,u,d,NULL,&v)){if(i)*i=v;if(n)*n=gc->rectangles[j]->normal;return gc->rectangles[j];}
		}
	}
	return NULL;
}

rectangle_struct* collideLineMapClosest(room_struct* r, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i)
{
	if(!r)return;
	listCell_struct *lc=r->rectangles.first;
	vect3D v;
	int32 lowestK=d;
	rectangle_struct* hit=NULL;
	while(lc)
	{
		if(&lc->data!=rec)
		{
			// NOGBA("%p vs %p",rec,&lc->data);
			int32 k;
			// if(collideLineRectangle(&lc->data,l,u,d,&k,&v)){if(k<lowestK){*i=v;lowestK=k;}}
			if(collideLineRectangle(&lc->data,l,u,lowestK,&k,&v)){if(k<lowestK){*i=v;lowestK=k;hit=&lc->data;}}
		}
		lc=lc->next;
	}
	return hit;
}

u8 computeLighting(vect3D l, int32 intensity, vect3D p, rectangle_struct* rec, room_struct* r)
{
	int32 dist=sqDistance(l,p);
	int32 rdist=sqrtf32(dist);
	dist=mulf32(dist,dist);
	// int32 dist=distance(l,p);
	if(dist<intensity)
	{
		vect3D u=vectDifference(p,l);
		u=divideVect(u,rdist);
		// NOGBA("dv : %d, %d, %d",u.x,u.y,u.z);
		if(collideLineMap(r, rec, l, u, rdist, NULL, NULL))return 0;
		int32 v=dotProduct(u,rec->normal);
		v=max(0,v);
		v*=3;
		v/=4;
		v+=inttof32(1)/4;
		// int32 v=inttof32(1);
		return mulf32(v,(31-((dist*31)/intensity)));
	}else return 0;
}

u8 computeLightings(entityCollection_struct* ec, vect3D p, rectangle_struct* rec, room_struct* r)
{
	if(ec)
	{
		int v=AMBIENTLIGHT;
		int i;
		for(i=0;i<ENTITYCOLLECTIONNUM;i++)
		{
			if(ec->entity[i].used && ec->entity[i].type==lightEntity && ec->entity[i].data)
			{
				entity_struct* e=&ec->entity[i];
				lightData_struct* d=ec->entity[i].data;
				// NOGBA("%d LIGHT ! %d, %d, %d, %d",i,e->position.x,e->position.y,e->position.z,d->intensity);
				v+=computeLighting(vect(e->position.x*TILESIZE*2,e->position.z*HEIGHTUNIT,e->position.y*TILESIZE*2), d->intensity, p, rec, r);
			}
		}
		return (u8)(31-min(max(v,0),31));
	}
	return 0;
}

void fillBuffer(u8* buffer, vect2D p, vect2D s, u8* v, bool rot, int w)
{
	int i;
	// u8 vt=(rand()%31)<<3;
	if(!rot)
	{
		for(i=0;i<s.x;i++)
		{
			int j;
			for(j=0;j<s.y;j++)
			{
				buffer[p.x+i+(p.y+j)*w]=v[i+j*s.x];
				// buffer[p.x+i+(p.y+j)*w]=vt;
			}
		}
	}else{
		for(i=0;i<s.x;i++)
		{
			int j;
			for(j=0;j<s.y;j++)
			{
				buffer[p.x+j+(p.y+i)*w]=v[i+j*s.x];
				// buffer[p.x+j+(p.y+i)*w]=vt;
			}
		}
	}
}

rectangle_struct* addRoomRectangle(room_struct* r, entityCollection_struct* ec, rectangle_struct rec, material_struct* mat, bool portalable)
{
	if((!rec.size.x && (!rec.size.z || !rec.size.y)) || (!rec.size.y && !rec.size.z))return NULL;
	initRectangle(&rec, rec.position, rec.size);
	if(mat)rec.material=mat;
	else{
		u16 x=rec.position.x;
		u16 y=rec.position.z;
		if(rec.size.x<0)x+=rec.size.x;
		if(rec.size.z<0)y+=rec.size.z;
		if(x>=r->width)x=r->width-1;
		if(y>=r->height)y=r->height-1;
		rec.material=r->materials[x+y*r->width];
	}
	// NOGBA("mat : %d",getMaterialID(rec.material));
	rec.portalable=portalable;
	return addRectangle(rec, &r->rectangles);
}

void generateLightmap(entityCollection_struct* ec, rectangle_struct* rec, room_struct* r, u8* b)
{
	if(rec && r->lightMap)// && rec->lightMap)
	{
		u16 x=rec->lmSize.x, y=rec->lmSize.y;
		// u16 x=rec->lightMap->width, y=rec->lightMap->height;
		u8* data=malloc(x*y);
		vect3D p=vect(rec->position.x*TILESIZE*2-TILESIZE,rec->position.y*HEIGHTUNIT,rec->position.z*TILESIZE*2-TILESIZE);
		NOGBA("p : %d, %d, %d",p.x,p.y,p.z);
		// u16 palette[256];
		// u16 palette[8];
		int i;
		// for(i=0;i<256;i++){u8 v=i%32;palette[i]=RGB15(v,v,v);}
		// for(i=0;i<8;i++){u8 v=0;palette[i]=RGB15(v,v,v);}
		vect3D u=getUnitVect(rec);
		for(i=0;i<x;i++)
		{
			int j;
			for(j=0;j<y;j++)
			{
				// data[i+j*rec->lightMap->width]=max(max(31-j,31-i),0);
				#ifdef A5I3
					if(!rec->size.x)data[i+j*x]=computeLightings(ec,addVect(p,vect(0,i*u.y+u.y/2,j*u.z+u.z/2)),rec,r)<<3;
					else if(rec->size.y)data[i+j*x]=computeLightings(ec,addVect(p,vect(i*u.x+u.x/2,j*u.y+u.y/2,0)),rec,r)<<3;
					else data[i+j*x]=computeLightings(ec,addVect(p,vect(i*u.x+u.x/2,0,j*u.z+u.z/2)),rec,r)<<3;
				#else
					if(!rec->size.x)data[i+j*x]=computeLightings(ec,addVect(p,vect(0,i*u.y+u.y/2,j*u.z+u.z/2)),rec,r);//<<3;
					else if(rec->size.y)data[i+j*x]=computeLightings(ec,addVect(p,vect(i*u.x+u.x/2,j*u.y+u.y/2,0)),rec,r);//<<3;
					else data[i+j*x]=computeLightings(ec,addVect(p,vect(i*u.x+u.x/2,0,j*u.z+u.z/2)),rec,r);//<<3;
				#endif
			}
		}
		NOGBA("p2");
		fillBuffer(b, vect2(rec->lmPos.x,rec->lmPos.y), vect2(rec->lmSize.x,rec->lmSize.y), data, rec->rot, r->lmSize.x);
		NOGBA("p3");
		// loadPartToBank(r->lightMap,data,x,y,rec->lmPos.x,rec->lmPos.y,rec->rot);
		// loadPaletteToBank(rec->lightMap,palette,8*2);
		free(data);
		NOGBA("p4");
	}else NOGBA("NOTHING?");
}

void generateLightmaps(roomEdit_struct* re, room_struct* r, entityCollection_struct* ec)
{
	if(!re || !r || !ec || re->lightUpToDate)return;
	listCell_struct *lc=r->rectangles.first;
	getRoomDoorRectangles(NULL, &re->data);
	rectangle2DList_struct rl;
	initRectangle2DList(&rl);
	while(lc)
	{
		insertRectangle2DList(&rl,(rectangle2D_struct){vect2(0,0),vect2(abs(lc->data.size.x?(lc->data.size.x*LIGHTMAPRESOLUTION):(lc->data.size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2))),abs((lc->data.size.y&&lc->data.size.x)?(lc->data.size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2)):(lc->data.size.z*LIGHTMAPRESOLUTION))),&lc->data,false});
		lc=lc->next;
	}
	int i;
	doorCollection_struct* dc=&doorCollection;
	for(i=0;i<NUMDOORS;i++)
	{
		if(dc->door[i].used)
		{
			door_struct* d=&dc->door[i];
			if(r==d->primaryRoom)
			{
				rectangle_struct* rec=&d->primaryRec;
				vect2D s=vect2(abs(rec->size.x?(rec->size.x*LIGHTMAPRESOLUTION):(rec->size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2))),abs((rec->size.y&&rec->size.x)?(rec->size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2)):(rec->size.z*LIGHTMAPRESOLUTION)));
				rec->lmSize=vect(s.x,s.y,0);
				insertRectangle2DList(&rl,(rectangle2D_struct){vect2(0,0),s,rec,false});
			}
			if(r==d->secondaryRoom)
			{
				rectangle_struct* rec=&d->secondaryRec;
				vect2D s=vect2(abs(rec->size.x?(rec->size.x*LIGHTMAPRESOLUTION):(rec->size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2))),abs((rec->size.y&&rec->size.x)?(rec->size.y*LIGHTMAPRESOLUTION*HEIGHTUNIT/(TILESIZE*2)):(rec->size.z*LIGHTMAPRESOLUTION))*LIGHTMAPRESOLUTION);
				rec->lmSize=vect(s.x,s.y,0);
				insertRectangle2DList(&rl,(rectangle2D_struct){vect2(0,0),s,rec,false});
			}
		}
	}
	short w=32, h=32;
	// packRectangles(&rl, 512, 256);
	// packRectangles(&rl, w, h);
	bool rr=packRectanglesSize(&rl, &w, &h);
	r->lmSize=vect(w,h,0);
	NOGBA("done : %d %dx%d",(int)rr,w,h);
	if(!r->lightMap)
	{
		#ifdef A5I3
			u16 palette[8];
			for(i=0;i<8;i++){u8 v=(i*31)/7;palette[i]=RGB15(v,v,v);}
			r->lightMap=createReservedTextureBufferA5I3(NULL,palette,w,h,(void*)(0x6800000+0x0020000));
		#else
			u16 palette[256];
			for(i=0;i<256;i++){u8 v=i%32;palette[i]=RGB15(v,v,v);}
			r->lightMap=createTextureBuffer(NULL,palette,w,h);
		#endif
	}else changeTextureSizeA5I3(r->lightMap,w,h);
	if(r->lightMapBuffer)free(r->lightMapBuffer);
	r->lightMapBuffer=malloc(w*h);
	// fillBuffer(buffer, vect2(0,0), vect2(512,256), 0, false);
	lc=r->rectangles.first;
	while(lc)
	{
		generateLightmap(ec, &lc->data, r, r->lightMapBuffer);
		lc=lc->next;
	}
	
	for(i=0;i<NUMDOORS;i++)
	{
		if(dc->door[i].used)
		{
			door_struct* d=&dc->door[i];
			if(r==d->primaryRoom)generateLightmap(ec, &d->primaryRec, r, r->lightMapBuffer);
			if(r==d->secondaryRoom)generateLightmap(ec, &d->secondaryRec, r, r->lightMapBuffer);
		}
	}
	
	NOGBA("done generating, loading...");
	loadToBank(r->lightMap,r->lightMapBuffer);
	NOGBA("loaded.");
	
	freeRectangle2DList(&rl);
	NOGBA("freed.");
	
	re->lightUpToDate=true;
}

void removeRectangles(room_struct* r)
{
	while(r->rectangles.num)popRectangle(&r->rectangles);
}

vect3D convertNormal(vect3D n, int32 v)
{
	if(n.x>0)n.x=v;
	else if(n.x<0)n.x=-v;
	if(n.y>0)n.y=v;
	else if(n.y<0)n.y=-v;
	if(n.z>0)n.z=v;
	else if(n.z<0)n.z=-v;
	return n;
}

vect3D getOverlayRectanglePosition(rectangle_struct rec)
{
	int32 v=LIGHTCONST;
	vect3D u=addVect(convertVect(rec.position),convertNormal(rec.normal,-LIGHTCONST));
	if(rec.size.x>0)u.x-=v;
	else if(rec.size.x)u.x+=v;
	if(rec.size.y>0)u.y-=v;
	else if(rec.size.y)u.y+=v;
	if(rec.size.z>0)u.z-=v;
	else if(rec.size.z)u.z+=v;
	return u;
}

vect3D getOverlayRectangleSize(rectangle_struct rec)
{
	int32 v=LIGHTCONST;
	vect3D u=convertSize(rec.size);
	if(u.x>0)u.x+=2*v;
	else if(u.x)u.x-=2*v;
	if(u.y>0)u.y+=2*v;
	else if(u.y)u.y-=2*v;
	if(u.z>0)u.z+=2*v;
	else if(u.z)u.z-=2*v;
	return u;
}

void drawRect(rectangle_struct rec, vect3D pos, vect3D size, bool c) //TEMP ? (clean up, switch to v10 ?)
{
	if(rec.hide)return;
	vect3D p1=vect(inttot16(rec.lmPos.x),inttot16(rec.lmPos.y),0);
	vect3D p2=vect(inttot16(rec.lmPos.x+rec.lmSize.x-1),inttot16(rec.lmPos.y+rec.lmSize.y-1),0);
	int32 t1=TEXTURE_PACK(p1.x, p1.y);
	int32 t2=TEXTURE_PACK(p1.x, p2.y);
	int32 t3=TEXTURE_PACK(p2.x, p2.y);
	int32 t4=TEXTURE_PACK(p2.x, p1.y);
	if(rec.rot)
	{
		p2=vect(inttot16(rec.lmPos.x+rec.lmSize.y-1),inttot16(rec.lmPos.y+rec.lmSize.x-1),0);
		t1=TEXTURE_PACK(p1.x, p1.y);
		t4=TEXTURE_PACK(p1.x, p2.y);
		t3=TEXTURE_PACK(p2.x, p2.y);
		t2=TEXTURE_PACK(p2.x, p1.y);
	}
	int32 t[4];
	if(c)
	{
		c=false;
		glColor3b(255,255,255);
		
		bindMaterial(rec.material,&rec,t,false);
		t1=t[0];
		t2=t[1];
		t3=t[2];
		t4=t[3];
	}
	if(!rec.size.x)
	{
		glBegin(GL_QUAD);
			GFX_TEX_COORD = t1;
			glVertex3v16(pos.x, pos.y, pos.z);

			GFX_TEX_COORD = t2;
			glVertex3v16(pos.x, pos.y, pos.z+size.z);

			GFX_TEX_COORD = t3;
			glVertex3v16(pos.x, pos.y+size.y, pos.z+size.z);

			GFX_TEX_COORD = t4;
			glVertex3v16(pos.x, pos.y+size.y, pos.z);
	}else if(rec.size.y){
		glBegin(GL_QUAD);
			GFX_TEX_COORD = t1;
			glVertex3v16(pos.x, pos.y, pos.z);

			GFX_TEX_COORD = t2;
			glVertex3v16(pos.x, pos.y+size.y, pos.z);

			GFX_TEX_COORD = t3;
			glVertex3v16(pos.x+size.x, pos.y+size.y, pos.z);

			GFX_TEX_COORD = t4;
			glVertex3v16(pos.x+size.x, pos.y, pos.z);
	}else{
		glBegin(GL_QUAD);
			GFX_TEX_COORD = t1;
			glVertex3v16(pos.x, pos.y, pos.z);

			GFX_TEX_COORD = t2;
			glVertex3v16(pos.x, pos.y, pos.z+size.z);

			GFX_TEX_COORD = t3;
			glVertex3v16(pos.x+size.x, pos.y, pos.z+size.z);

			GFX_TEX_COORD = t4;
			glVertex3v16(pos.x+size.x, pos.y, pos.z);
	}
}

void drawRectDL(rectangle_struct rec, vect3D pos, vect3D size, bool c, vect3D cpos, vect3D cnormal, bool cull) //TEMP ?
{
	if(rec.hide)return;
	
	vect3D v[4];
	if(!rec.size.x)
	{
		v[0]=vect(pos.x, pos.y, pos.z);
		v[1]=vect(pos.x, pos.y, pos.z+size.z);
		v[2]=vect(pos.x, pos.y+size.y, pos.z+size.z);
		v[3]=vect(pos.x, pos.y+size.y, pos.z);
	}else if(rec.size.y){
		v[0]=vect(pos.x, pos.y, pos.z);
		v[1]=vect(pos.x, pos.y+size.y, pos.z);
		v[2]=vect(pos.x+size.x, pos.y+size.y, pos.z);
		v[3]=vect(pos.x+size.x, pos.y, pos.z);
	}else{
		v[0]=vect(pos.x, pos.y, pos.z);
		v[1]=vect(pos.x, pos.y, pos.z+size.z);
		v[2]=vect(pos.x+size.x, pos.y, pos.z+size.z);
		v[3]=vect(pos.x+size.x, pos.y, pos.z);
	}
	
	if(cull)
	{
		int i;bool pass=false;
		for(i=0;i<4;i++)
		{
			vect3D v1=vectDifference(v[i],cpos);
			if(dotProduct(v1,cnormal)>0)pass=true;
		}
		if(!pass)return;
	}
	
	vect3D p1=vect(inttot16(rec.lmPos.x),inttot16(rec.lmPos.y),0);
	vect3D p2=vect(inttot16(rec.lmPos.x+rec.lmSize.x-1),inttot16(rec.lmPos.y+rec.lmSize.y-1),0);
	int32 t1=TEXTURE_PACK(p1.x, p1.y);
	int32 t2=TEXTURE_PACK(p1.x, p2.y);
	int32 t3=TEXTURE_PACK(p2.x, p2.y);
	int32 t4=TEXTURE_PACK(p2.x, p1.y);
	if(rec.rot)
	{
		p2=vect(inttot16(rec.lmPos.x+rec.lmSize.y-1),inttot16(rec.lmPos.y+rec.lmSize.x-1),0);
		t1=TEXTURE_PACK(p1.x, p1.y);
		t4=TEXTURE_PACK(p1.x, p2.y);
		t3=TEXTURE_PACK(p2.x, p2.y);
		t2=TEXTURE_PACK(p2.x, p1.y);
	}
	int32 t[4];
	if(c)
	{
		c=false;
		glColor3b(255,255,255);
		
		bindMaterial(rec.material,&rec,t,true);
		t1=t[0];
		t2=t[1];
		t3=t[2];
		t4=t[3];
	}
	glBeginDL(GL_QUAD);
		glTexCoordPACKED(t1);
		glVertex3v16DL(v[0].x, v[0].y, v[0].z);

		glTexCoordPACKED(t2);
		glVertex3v16DL(v[1].x, v[1].y, v[1].z);

		glTexCoordPACKED(t3);
		glVertex3v16DL(v[2].x, v[2].y, v[2].z);

		glTexCoordPACKED(t4);
		glVertex3v16DL(v[3].x, v[3].y, v[3].z);
}

s16 transferRectangle(vect3D pos, vect3D size, vect3D normal)
{
	if(size.x<0){pos.x+=size.x;size.x=-size.x;}
	if(size.y<0){pos.y+=size.y;size.y=-size.y;}
	if(size.z<0){pos.z+=size.z;size.z=-size.z;}
	return createAAR(vectMultInt(size,4), vectMultInt(pos,4), vectMultInt(normal,-1));
}

void transferRectangles(room_struct* r)
{
	listCell_struct *lc=r->rectangles.first;
	int i=0;
	while(lc)
	{
		lc->data.AARid=transferRectangle(addVect(convertSize(vect(r->position.x,0,r->position.y)),convertVect(lc->data.position)),convertSize(lc->data.size),lc->data.normal);
		lc=lc->next;
		i++;
		if(!(i%8))swiWaitForVBlank();
	}
}

void drawRectangles(room_struct* r, u8 mode, u16 color)
{
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);
	
	listCell_struct *lc=r->rectangles.first;
	unbindMtl();
	GFX_COLOR=RGB15(31,31,31);
	int i=0;
	while(lc)
	{
		drawRect(lc->data,convertVect(lc->data.position),convertSize(lc->data.size),true);
		i++;
		lc=lc->next;
	}
	if(mode&6)
	{
		lc=r->rectangles.first;
		glPolyFmt(POLY_ALPHA(31) | (1<<14) | POLY_CULL_BACK);
		if(mode&2)applyMTL(r->lightMap);
		else if(r->lmSlot)applyMTL(r->lmSlot->mtl);
		GFX_COLOR=RGB15(31,31,31);
		while(lc)
		{
			drawRect(lc->data,convertVect(lc->data.position),convertSize(lc->data.size),false);
			lc=lc->next;
		}
	}
	if(mode&128 && color)
	{
		lc=r->rectangles.first;
		glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT);
		unbindMtl();
		GFX_COLOR=color;
		while(lc)
		{
			drawRect(lc->data,convertVect(lc->data.position),convertSize(lc->data.size),false);
			lc=lc->next;
		}
	}
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);
}

bool compare(room_struct* r, u8 v, int i, int j)
{
	if(i<0 || j<0 || i>r->width-1 || j>r->height-1)return false;
	u8 v2=r->floor[i+j*r->width];
	u8 v3=r->ceiling[i+j*r->width];
	if(v2>=v3)return false;
	return (v2<=v+MAXSTEP);
}

void getPathfindingData(room_struct* r)
{
	if(!r)return;
	// if(r->pathfindingData)free(r->pathfindingData);
	r->pathfindingData=malloc(sizeof(u8)*r->width*r->height);
	if(!r->pathfindingData)return;
	int i, j;
	for(i=0;i<r->width;i++)
	{
		for(j=0;j<r->height;j++)
		{
			u8 v=r->floor[i+j*r->width];
			u8* v2=&r->pathfindingData[i+j*r->width];
			*v2=0;
			if(compare(r, v, i, j-1))*v2|=DIRUP;
			if(compare(r, v, i, j+1))*v2|=DIRDOWN;
			if(compare(r, v, i-1, j))*v2|=DIRLEFT;
			if(compare(r, v, i+1, j))*v2|=DIRRIGHT;
			// if(i==5)NOGBA("v %d : %d %d %d %d",j,(*v2)&DIRUP,(*v2)&DIRDOWN,(*v2)&DIRLEFT,(*v2)&DIRRIGHT);
			// *v2=DIRUP|DIRDOWN|DIRLEFT|DIRRIGHT;
		}
	}
	NOGBA("got PF data %d %d",r->width,r->height);
}

void initRoomGrid(room_struct* r)
{
	if(!r)return;
	
	r->rectangleGridSize.x=r->width/CELLSIZE+1;
	r->rectangleGridSize.y=r->height/CELLSIZE+1;
	
	r->rectangleGrid=malloc(sizeof(gridCell_struct)*r->rectangleGridSize.x*r->rectangleGridSize.y);
	
	int i;
	for(i=0;i<r->rectangleGridSize.x*r->rectangleGridSize.y;i++)
	{
		r->rectangleGrid[i].rectangles=NULL;
	}
}

void initRoom(room_struct* r, u16 w, u16 h, vect3D p)
{
	if(!r)return;
	
	r->width=w;
	r->height=h;
	r->position=p;
	
	r->lmSlot=NULL;
	
	initRectangleList(&r->rectangles);
	
	if(r->height && r->width)
	{
		r->floor=malloc(r->height*r->width);
		r->ceiling=malloc(r->height*r->width);
		r->materials=malloc(r->height*r->width*sizeof(material_struct*));
		int i;for(i=0;i<r->height*r->width;i++){r->floor[i]=DEFAULTFLOOR;r->ceiling[i]=DEFAULTCEILING;r->materials[i]=NULL;}
	}else {r->floor=r->ceiling=NULL;}
	
	r->lightMap=NULL;
	r->lightMapBuffer=NULL;
	r->pathfindingData=NULL;
	r->doorWay=NULL;
		
	initRoomGrid(r);
}

gridCell_struct* getCurrentCell(room_struct* r, vect3D o)
{
	if(!r)return NULL;
	
	o=vectDifference(o,convertSize(vect(r->position.x,0,r->position.y)));
	o=vectDivInt(o,CELLSIZE*TILESIZE*2);
	
	if(o.x>=0 && o.x<r->rectangleGridSize.x && o.z>=0 && o.z<r->rectangleGridSize.y)return &r->rectangleGrid[o.x+o.z*r->rectangleGridSize.x];
	return NULL;
}

void drawCell(gridCell_struct* gc) //debug
{
	if(!gc)return;
	
	room_struct* r=getPlayer()->currentRoom;
	if(!r)return;
	glPushMatrix();
	glTranslate3f32(TILESIZE*2*r->position.x, 0, TILESIZE*2*r->position.y);
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);
	int i;
	for(i=0;i<gc->numRectangles;i++)
	{
		drawRect(*gc->rectangles[i],convertVect(gc->rectangles[i]->position),convertSize(gc->rectangles[i]->size),true);
	}
	glPopMatrix(1);
}

void generateGridCell(room_struct* r, gridCell_struct* gc, u16 x, u16 y)
{
	if(!r || !gc)return;
	
	if(gc->rectangles)free(gc->rectangles);
	gc->numRectangles=0;
	gc->lights[0]=NULL;gc->lights[1]=NULL;gc->lights[2]=NULL;
	
	x*=CELLSIZE*2;y*=CELLSIZE*2; //so getting the center isn't a problem
	x+=CELLSIZE;y+=CELLSIZE; //center
	
	listCell_struct *lc=r->rectangles.first;
	while(lc)
	{
		if((abs(x-(lc->data.position.x*2+lc->data.size.x))<=(CELLSIZE+abs(lc->data.size.x))) && (abs(y-(lc->data.position.z*2+lc->data.size.z))<=(CELLSIZE+abs(lc->data.size.z))))
		{
			gc->numRectangles++;
		}
		lc=lc->next;
	}
	gc->rectangles=malloc(sizeof(rectangle_struct*)*gc->numRectangles);
	gc->numRectangles=0;
	lc=r->rectangles.first;
	while(lc)
	{
		if((abs(x-(lc->data.position.x*2+lc->data.size.x))<=(CELLSIZE+abs(lc->data.size.x))) && (abs(y-(lc->data.position.z*2+lc->data.size.z))<=(CELLSIZE+abs(lc->data.size.z))))
		{
			gc->rectangles[gc->numRectangles++]=&lc->data;
		}
		lc=lc->next;
	}
	
	getClosestLights(r->entityCollection, vect(x/2,0,y/2), &gc->lights[0], &gc->lights[1], &gc->lights[2], &gc->lightDistances[0], &gc->lightDistances[1], &gc->lightDistances[2]);
}

void generateRoomGrid(room_struct* r)
{
	if(!r)return;
	
	int i, j;
	for(i=0;i<r->rectangleGridSize.x;i++)
	{
		for(j=0;j<r->rectangleGridSize.y;j++)
		{
			generateGridCell(r,&r->rectangleGrid[i+j*r->rectangleGridSize.x],i,j);
		}
	}
}

u8 getHeightValue(room_struct* r, vect3D pos, bool floor)
{
	if(!r)return 0;
	if(pos.x<0 || pos.z<0 || pos.x>=r->width || pos.z>=r->height)return 0;
	return floor?(r->floor[pos.x+pos.z*r->width]):(r->ceiling[pos.x+pos.z*r->width]);
}

void getDoorWayData(doorCollection_struct* dc, room_struct* r)
{
	if(!r || r->doorWay)return;
	if(!dc)dc=&doorCollection;
	r->doorWay=malloc(sizeof(void*)*(r->width+r->height)*4);
	int i, j;
	void** dw=r->doorWay;
	for(j=0;j<2;j++)for(i=0;i<r->width;i++)
	{
		door_struct* d=inDoorWay(NULL, r, i, j-2, false, true);
		if(d){dw[i+j*r->width]=(void*)d;}
		else
		{
			d=inDoorWay(NULL, r, i, j-2, true, true);
			if(d){dw[i+j*r->width]=(void*)d;}
			else dw[i+j*r->width]=NULL;
		}
	}
	dw=&r->doorWay[r->width*2];
	
	for(j=0;j<2;j++)for(i=0;i<r->width;i++)
	{
		door_struct* d=inDoorWay(NULL, r, i, r->height+j, false, true);
		if(d)dw[i+j*r->width]=(void*)d;
		else
		{
			d=inDoorWay(NULL, r, i, r->height+j, true, true);
			if(d){dw[i+j*r->width]=(void*)d;}
			else dw[i+j*r->width]=NULL;
		}
	}
	
	dw=&r->doorWay[r->width*4];
	
	for(j=0;j<2;j++)for(i=0;i<r->height;i++)
	{
		door_struct* d=inDoorWay(NULL, r, j-2, i, false, true);
		if(d)dw[i+j*r->height]=(void*)d;
		else
		{
			d=inDoorWay(NULL, r, j-2, i, true, true);
			if(d)dw[i+j*r->height]=(void*)d;
			else dw[i+j*r->height]=NULL;
		}
	}
	
	dw=&r->doorWay[r->width*4+r->height*2];
	
	for(j=0;j<2;j++)for(i=0;i<r->height;i++)
	{
		door_struct* d=inDoorWay(NULL, r, r->width+j, i, false, true);
		if(d)dw[i+j*r->height]=(void*)d;
		else
		{
			d=inDoorWay(NULL, r, r->width+j, i, true, true);
			if(d)dw[i+j*r->height]=(void*)d;
			else dw[i+j*r->height]=NULL;
		}
	}
}

void resizeRoom(room_struct* r, u16 w, u16 h, vect3D p)
{
	if(r)
	{
		if(!r->height || ! r->width || !r->floor || !r->ceiling){initRoom(r, w, h, p);return;}
		int i, j;
		int h2=h, w2=w;
		r->position=p;
		u8* temp1=r->floor;
		u8* temp2=r->ceiling;
		material_struct** temp3=r->materials;
		r->floor=malloc(h*w);
		r->ceiling=malloc(h*w);
		r->materials=malloc(sizeof(material_struct*)*h*w);
		if(w>r->width)w2=r->width;
		if(h>r->height)h2=r->height;
		for(j=0;j<h2;j++){for(i=0;i<w2;i++){r->floor[i+j*w]=temp1[i+j*r->width];r->ceiling[i+j*w]=temp2[i+j*r->width];r->materials[i+j*w]=temp3[i+j*r->width];}for(;i<w;i++){r->floor[i+j*w]=DEFAULTFLOOR;r->ceiling[i+j*w]=DEFAULTCEILING;r->materials[i+j*w]=NULL;}}
		for(;j<h;j++)for(i=0;i<w;i++){r->floor[i+j*w]=DEFAULTFLOOR;r->ceiling[i+j*w]=DEFAULTCEILING;r->materials[i+j*w]=NULL;}
		r->height=h;
		r->width=w;
		free(temp1);
		free(temp2);
		free(temp3);
	}
}

void drawRoom(room_struct* r, u8 mode, u16 color) //obviously temp
{
	if(!r)return;
	int i, j;
	u8* f=r->floor;
	if(mode&8)
	{
		room_struct* r2=getCurrentRoom();
		if(r->lmSlot && r2 && r!=r2 && !isDoorOpen(NULL,r,r2)){NOGBA("HAPPENED");r->lmSlot->used=false;r->lmSlot=NULL;}
		if(r->lmSlot && !r->lmSlot->used)r->lmSlot=NULL;
		if(!r->lmSlot)return;
	}
	glPushMatrix();
		glTranslate3f32(TILESIZE*2*r->position.x, 0, TILESIZE*2*r->position.y);
		if(mode&1)drawRectangles(r, mode, color);
		else{
			unbindMtl();
			glBegin(GL_QUAD);
			for(j=0;j<r->height;j++)
			{
				for(i=0;i<r->width;i++)
				{
					s16 v=HEIGHTUNIT*(*(f++));
					s16 x=TILESIZE*2*i, y=TILESIZE*2*j;
					glColor3b(255,0,0);
					glVertex3v16(-TILESIZE+x, v, -TILESIZE+y);

					glColor3b(0,255,0);
					glVertex3v16(TILESIZE+x, v, -TILESIZE+y);

					glColor3b(0,0,255);
					glVertex3v16(TILESIZE+x, v, TILESIZE+y);

					glColor3b(255,0,255);
					glVertex3v16(-TILESIZE+x, v, TILESIZE+y);
				}
			}
			
			f=r->ceiling;
			
			for(j=0;j<r->height;j++)
			{
				for(i=0;i<r->width;i++)
				{
					s16 v=HEIGHTUNIT*(*(f++));
					s16 x=TILESIZE*2*i, y=TILESIZE*2*j;
					glColor3b(255,0,0);
					glVertex3v16(-TILESIZE+x, v, -TILESIZE+y);

					glColor3b(0,255,0);
					glVertex3v16(TILESIZE+x, v, -TILESIZE+y);

					glColor3b(0,0,255);
					glVertex3v16(TILESIZE+x, v, TILESIZE+y);

					glColor3b(255,0,255);
					glVertex3v16(-TILESIZE+x, v, TILESIZE+y);
				}
			}
			glEnd();
		}
	glPopMatrix(1);
}

u32* generateRoomDisplayList(room_struct* r, vect3D pos, vect3D normal, bool cull)
{	
	if(!r)r=getPlayer()->currentRoom;
	if(!r)return NULL;
	u32* ptr=glBeginListDL();
	
	glPolyFmtDL(POLY_ALPHA(31) | POLY_CULL_BACK);
	listCell_struct *lc=r->rectangles.first;
	while(lc)
	{
		drawRectDL(lc->data,convertVect(lc->data.position),convertSize(lc->data.size),true,vectDifference(pos,vect(TILESIZE*2*r->position.x, 0, TILESIZE*2*r->position.y)),normal,cull);
		lc=lc->next;
	}
	
	lc=r->rectangles.first;
	glPolyFmtDL(POLY_ALPHA(31) | (1<<14) | POLY_CULL_BACK);
	applyMTLDL(r->lightMap);
	while(lc)
	{
		drawRectDL(lc->data,convertVect(lc->data.position),convertSize(lc->data.size),false,vectDifference(pos,vect(TILESIZE*2*r->position.x, 0, TILESIZE*2*r->position.y)),normal,cull);
		lc=lc->next;
	}
	
	u32 size=glEndListDL();
	u32* displayList=malloc((size+1)*4);
	if(displayList)memcpy(displayList,ptr,(size+1)*4);
	return displayList;
}

vect3D getVector(vect3D pos, entity_struct* ent)
{
	vect3D p=ent->position;
	vect3D v=vectDifference(pos,vect(p.x*(TILESIZE*2),p.z*HEIGHTUNIT,p.y*(TILESIZE*2)));
	v=vectMultInt(v,100);
	int32 dist=magnitude(v);
	v=divideVect(v,dist);
	// dist/=100;
	return vect(f32tov10(v.x),f32tov10(v.y),f32tov10(v.z));
}

void setupObjectLighting(room_struct* r, vect3D pos, u32* params)
{
	if(!r)r=getPlayer()->currentRoom;
	if(!r)return;
	entity_struct *l1, *l2, *l3;
	int32 d1, d2, d3;
	vect3D tilepos=reverseConvertVect(vectDifference(pos,convertVect(vect(r->position.x,0,r->position.y))));
	// getClosestLights(r->entityCollection, tilepos, &l1, &l2, &l3, &d1, &d2, &d3);
	gridCell_struct* gc=getCurrentCell(r, pos);
	if(!gc)return;
	l1=gc->lights[0];
	l2=gc->lights[1];
	l3=gc->lights[2];
	d1=gc->lightDistances[0];
	d2=gc->lightDistances[1];
	d3=gc->lightDistances[2];
	// *params=POLY_ALPHA(31) | POLY_CULL_FRONT;
	
	glMaterialf(GL_AMBIENT, RGB15(5,5,5));
	glMaterialf(GL_DIFFUSE, RGB15(31,31,31));
	glMaterialf(GL_SPECULAR, RGB15(0,0,0));
	glMaterialf(GL_EMISSION, RGB15(0,0,0));
	
	if(l1)
	{
		*params|=POLY_FORMAT_LIGHT0;
		vect3D v=getVector(pos, l1);
		d1*=64;
		int32 v2=31-((31*d1)*(((lightData_struct*)l1->data)->intensity));
		glLight(0, RGB15(v2,v2,v2), v.x, v.y, v.z);
		if(l2)
		{
			*params|=POLY_FORMAT_LIGHT1;
			vect3D v=getVector(pos, l2);
			int32 v2=31-((31*d2)*(((lightData_struct*)l2->data)->intensity));
			glLight(1, RGB15(v2,v2,v2), v.x, v.y, v.z);
			if(l3)
			{
				*params|=POLY_FORMAT_LIGHT2;
				vect3D v=getVector(pos, l3);
				int32 v2=31-((31*d3)*(((lightData_struct*)l3->data)->intensity));
				glLight(2, RGB15(v2,v2,v2), v.x, v.y, v.z);
			}
		}
	}
}

void freeRoom(room_struct* r)
{
	if(r)
	{
		if(r->floor)free(r->floor);
		if(r->ceiling)free(r->ceiling);
		if(r->materials)free(r->materials);
		if(r->rectangleGrid)
		{
			int i;
			for(i=0;i<r->rectangleGridSize.x*r->rectangleGridSize.y;i++)
			{
				if(r->rectangleGrid[i].rectangles)free(r->rectangleGrid[i].rectangles);
			}
			free(r->rectangleGrid);
		}
		r->floor=NULL;
		r->ceiling=NULL;
		r->materials=NULL;
		if(r->lightMapBuffer)free(r->lightMapBuffer);
		r->lightMapBuffer=NULL;
		if(r->pathfindingData)free(r->pathfindingData);
		r->pathfindingData=NULL;
		if(r->doorWay)free(r->doorWay);
		r->doorWay=NULL;
		if(r->lightMap)r->lightMap->used=false;
		removeRectangles(r);
	}
}
