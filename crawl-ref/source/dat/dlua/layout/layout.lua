------------------------------------------------------------------------------
-- layout.lua: General layout functions and utilities
--
------------------------------------------------------------------------------

layout = {}

layout.area_in_bounds = function(bounds,opt)

  local minpadx,minpady,minsizex,minsizey = 0,0,1,1

  if opt.min_pad ~= nil then minpadx,minpady = opt.min_pad,opt.min_pad end
  if opt.min_size ~= nil then minsizex,minsizey = opt.min_size,opt.min_size end

  local width,height = bounds.x2-bounds.x1+1,bounds.y2-bounds.y1+1
  local maxx,maxy = width - 2*minpadx, height - 2*minpady

  if maxx<minsizex or maxy<minsizey then return nil end

  local sizex,sizey = crawl.random_range(minsizex,maxx),crawl.random_range(minsizey,maxy)
  local posx,posy = crawl.random_range(0,maxx-sizex),crawl.random_range(0,maxy-sizey)
  return { x1 = bounds.x1 + minpadx + posx, y1 = bounds.y1 + minpady + posy,
           x2 = bounds.x1 + minpadx + posx + sizex - 1, y2 = bounds.y1 + minpady + posy + sizey - 1 }

end
