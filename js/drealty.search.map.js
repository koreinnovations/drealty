(function ($) {
  var map;
  var markers = [];
  $( function() {
    
    var mapCenter = new google.maps.LatLng(45.1219444,-95.0430556);
    var mapOptions = {
      zoom: 12,
      center: mapCenter,
      mapTypeId: google.maps.MapTypeId.ROADMAP
    };
    
    map = new google.maps.Map(document.getElementById("map_canvas"), mapOptions);
    google.maps.event.addListener(map, 'idle', function() {
      var x = map.getBounds();
      $("#bounds").html( "north east:" + x.getNorthEast().toString() + "  south west: " + x.getSouthWest().toString());
      
      var bounds = map.getBounds();
      var bb;
      bb = new Object();
      north_east = new Object();
      south_west = new Object();
      
      north_east.lat = bounds.getNorthEast().lat();
      north_east.lng = bounds.getNorthEast().lng();
      south_west.lat = bounds.getSouthWest().lat();
      south_west.lng = bounds.getSouthWest().lng();
      
      bb.north_east = north_east;
      bb.south_west = south_west;
      
      bb.distance = 12;
      bb.zoom = map.getZoom();
      
      $.get("/drealty/js/map",  {
        bounding_box:bb
      }, function(data) {
        
         
        if ( markers ) {
          for( x in markers) {
            markers[x].setMap(null);
          }
          // empty the array
          markers.length = 0;
        }


          
        var count = data.length;
        for( var i = 0; i < count; i++) {
          
          var pt = new google.maps.LatLng(data[i].center.lat, data[i].center.lng);
          
          var iconURL = (data[i].count == 1) ? "/sites/all/modules/drealty/img/single_house.png" : "/sites/all/modules/drealty/img/cluster_house.png"

          var marker = new google.maps.Marker({
            position: pt,
            map: map,
            icon: iconURL
          });
            
          markers.push(marker);
            
        }
      },
      "json");
      
    })
    

  
  });

})(jQuery);