(function(in_str) { 
    var data_valid = false;
    var tick_val = 0;
    var type_val = "null";
    var data_val = 0;
    var graph_val = false;
    
    // Remove stuff in square brackets (including the brackets)
    in_str = in_str.replace(/\[[^\[]*\]/g,'');
    
    // Remove leading & trailing whitespace
    in_str = in_str.trim();
    
    // Split into array/list using space (' ') as delimiter
    var in_items = in_str.split(" ")
    
    if (in_items.length > 0) {
        tick_val = in_items[0]
    }
    if (in_items.length > 1) {
        type_val = in_items[1]
    }
    if (in_items.length > 2) {
        data_valid = true;
        switch (type_val) {
            // Qidata: handle remaining objects as hex string
            case "qidata":
                data_val = in_items.slice(2).join(" ");
                graph_val = false;
                break;
            // State: handle remaining elements as a string
            case "state":
                data_val = in_items.slice(2).join(" ");
                graph_val = false;
                break;
            // Freq: next element is a number
            case "freq":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Dut: next element is a number
            case "dut":
                data_val = in_items[2];
                graph_val = true;
                break;
            // DC Volt: next element is a number
            case "dcvolt":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Icrms: next element is a number
            case "Icrms":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Ptx: next element is a number
            case "ptx":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Prx: next element is a number
            case "prx":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Ppad: next element is a number
            case "ppad":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Pfor: next element is a number
            case "pfor":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Temp: next element is a number
            case "temp":
                data_val = in_items[2];
                graph_val = true;
                break;
            // Non-matching data (ignore)
            default: 
                data_valid = false;
                break;
        }
    }
    
    // Return the output object per Serial-Studio specification
    if (data_valid) {
        return {
            t: "node",
            g: [ {
                t: type_val,
                d: [ {
                    v: data_val,
                    g: graph_val
                } ]
            } ]
        };
    }
    else {
        return { };
    }
})

// For testing via node
//console.log(parse("[2020-10-18 09:01:50.141] 110 qidata 01 72 73"))
