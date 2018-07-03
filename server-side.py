import datetime
import sqlite3
import requests
#import matplotlib.pyplot as plt
import numpy as np
from bokeh.plotting import figure, output_file, show
from bokeh.embed import components
from bokeh.models import Title




heartRate_db = "__HOME__/heartRate.db"

def request_handler(request):
    if (request["method"] == 'POST'):
        return do_POST(request)
    elif (request["method"] == 'GET'):
        if ('type' not in request["args"]):
            return "Error please enter the type of graph"
        else:
            #data from SQL
            data = get_data()
            
            #type of graph from input
            graph = request['values']['type']
            if graph == "ekg":
                index = 0
                t = ("Graph for the EKG")
            elif graph == "steps":
                index = 1
                t = ("Graph for the Steps")
            else:
                return "error please enter either ekg or steps"
            
            #time axis
            xaxis = []
            for i in range(len(data[index])+1):
                xaxis.append(i*5)
                
            #bokeh stuff
            p = figure(title = t, plot_width=400, plot_height=400)
            p.line(xaxis, data[index], line_width=2)

            #titles
            p.add_layout(Title(text="Time (s)", align="center"), "below")
            if index == 0:
                p.add_layout(Title(text="Heart Rate (BPM)", align="left"), "left")
            else:
                p.add_layout(Title(text="Steps (steps per second)", align="left"), "left")
            p.title.text_font_size = "20px"
            
            
            p.toolbar.logo = None
            script, div = components(p)
            output = r"""<!DOCTYPE html>
        <html lang="en">
            <head>
                <meta charset="utf-8">
                <title>Bokeh Scatter Plots</title>

                <link rel="stylesheet" href="http://cdn.pydata.org/bokeh/release/bokeh-0.12.13.min.css" type="text/css" />
                <script type="text/javascript" src="http://cdn.pydata.org/bokeh/release/bokeh-0.12.13.min.js"></script>
                {}

            </head>
            <body>
                {}
            </body>
        </html>""".format(script,div)
            return output




def do_POST(request):
    kerb = str(request['form']["kerb"])
    heartRate = request['form']["heartRate"]
    steps = request['form']["steps"]
    
    
    #SQL stuff
    conn = sqlite3.connect(heartRate_db)  # connect to that database (will create if it doesn't already exist)
    c = conn.cursor()  # make cursor into database (allows us to execute commands)
    outs = ""
    try:
        c.execute('''CREATE TABLE dated_table (ID, time, kerb, rate, steps);''') # run a CREATE TABLE command
        outs = "database constructed"
    except:
        things = c.execute('''SELECT * FROM dated_table ORDER BY ID DESC;''').fetchone()

        if things == None:
            things=[0]
            
        dataID = things[0] + 1
        
        c.execute('''INSERT into dated_table VALUES (?,?,?,?,?);''', (dataID,datetime.datetime.now(),kerb,heartRate,steps))
        things = c.execute('''SELECT * FROM dated_table ORDER BY ID DESC;''').fetchall()
        outs = things
    conn.commit() # commit commands
    conn.close() # close connection to database
    return outs

def get_data():
    #SQL stuff
    conn = sqlite3.connect(heartRate_db)  # connect to that database (will create if it doesn't already exist)
    c = conn.cursor()  # make cursor into database (allows us to execute commands)
    outs = ""

    things = c.execute('''SELECT * FROM dated_table ORDER BY ID DESC;''').fetchone()

    result = []
    result.append(parse(tokenize(things[3])))
    result.append(parse(tokenize(things[4])))
    conn.commit() # commit commands
    conn.close() # close connection to database
    return result

def tokenize(equation):
    """helper function: splits up function with no spaces"""
    token = []
    #keeps track of current "word"
    val = ""
    for i in equation:
        #if there is a paren
        if i == ",":
            pass
        elif i == "[":
            if val != "":
                token.append(val)
                val = ""
            token.append(i)
        elif i == "]":
            if val != "":
                token.append(val)
                val = ""
            val += i
        #keeps track of op and numbers 
        elif i != " ":
            val += i
        elif i == " ":
            token.append(val)
            val = ""
    if val != "":
        token.append(val)
    return token


def parse(tokens):
    """
    Parses a list of tokens, constructing a representation where:
        * symbols are represented as Python strings
        * numbers are represented as Python ints or floats
        * S-expressions are represented as Python lists

    Arguments:
        tokens (list): a list of strings representing tokens
    """
    # (define circle-area (lambda (r) (* 3.14 (* r r))))
    # ['define', 'circle-area', ['lambda', ['r'], ['*', 3.14, ['*', 'r', 'r']]]]
    def check_valid_paren(s):
        """return True if each left parenthesis is closed by exactly one
        right parenthesis later in the string and each right parenthesis
        closes exactly one left parenthesis earlier in the string."""
        par = 0
        end_par = False
        for i in s:
            if i == "[":
                par += 1
                end_par = True
            elif i == "]":
                par -= 1
                end_par = False
            if par<0:
                return False
        #checks if it ends with an open paren or if the number of open doesn't equal close paren
        if end_par or par != 0:
            return False
        return True
    def typeChange(x):
        """helper function: to change type into a int float or stay as a string"""
        try:
            #if float or int
            a = float(x)
            try:
                b = int(x)
                if a == b:
                    return b
            except:
                return a
        except:
            #else it's a str
            return x
    #if str, float, or int
    if tokens[0] != "[":
        if len(tokens) == 1:
            return typeChange(tokens[0])
        raise SyntaxError
    #if s-expression
    if not check_valid_paren(tokens):
        raise SyntaxError
    #parses s expressions
    def parseExpressions(tokens, index = 1):
        """helper function: recursive function that parses s - expressions"""
        # (define circle-area (lambda (r) (* 3.14 (* r r))))
        parsed = []
        while index<len(tokens):
            item = tokens[index]
            # if its an ( then there needs to be a new list so it recursively calls itself
            if item == "[":
                inner_list, index = parseExpressions(tokens, index+1)
                parsed.append(inner_list)
            elif item == "]":
                return parsed, index + 1
            else:
                parsed.append(typeChange(item))
                index += 1
        return parsed, index
    parsed, index = parseExpressions(tokens)
    return parsed

##def ekgPlot(data):
##    plt.xlabel('time/ s')
##    plt.ylabel('Heart rate/ bpm')
##    plt.title('EKG graph')
##    
##    #since we take a heartrate reading once every 0.5 seconds
##    xaxis = list(np.arange(0, len(data)//2, 0.5))
##    
##    plt.plot(xaxis,data,'o')
##    plt.plot(xaxis,data)
##    plt.axis([0,-(-len(data)//2),0,max(data)+-(-max(data)//10)])
##    plt.show()

##def ekgPlot2(data):
##    p = figure(plot_width=400, plot_height=400)
##    xaxis = []
##    for i in range(len(data)+1):
##        xaxis.append(i*5)
##    p.line(xaxis, data, line_width=2)
##    p.toolbar.logo = None
##    script, div = components(p)
##    output = r"""<!DOCTYPE html>
##<html lang="en">
##    <head>
##        <meta charset="utf-8">
##        <title>Bokeh Scatter Plots</title>
##
##        <link rel="stylesheet" href="http://cdn.pydata.org/bokeh/release/bokeh-0.12.13.min.css" type="text/css" />
##        <script type="text/javascript" src="http://cdn.pydata.org/bokeh/release/bokeh-0.12.13.min.js"></script>
##        {}
##
##    </head>
##    <body>
##        {}
##    </body>
##</html>""".format(script,div)
##    return output

##def stepPlot(data):
##    plt.xlabel('time/ s')
##    plt.ylabel('Steps/ steps per min')
##    plt.title('Steps graph')
##    
##    #since we take a steps reading once every 5 seconds
##    xaxis = list(np.arange(0, len(data)*5, 5))
##    
##    plt.plot(xaxis,data,'o')
##    plt.plot(xaxis,data)
##    plt.axis([0,-(-len(data)*5),0,max(data)+-(-max(data)//10)])
##    plt.show()



##stepPlot(random.sample(range(160, 170), 8))
##ekgPlot(random.sample(range(120, 145), 8))


