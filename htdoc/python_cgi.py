import cgi, cgitb, os

form = cgi.FieldStorage()

first_name = form.getvalue("first_name")
last_name  = form.getvalue("last_name")

print ("Content-type:text/html\r\n\r\n")

print ("<html>")
print ("<head>")
print ("</head>")
print ("<body>")
print ("<h1>Hello {0} {1} from python_cgi!</h1>".format(first_name, last_name))
print ("<h2>environment variable:</h2>")
print ("<ul>")
for key in os.environ.keys():
    print ("<li><span style='color:green'> {0} </span> : {1} </li>".format(key, os.environ[key]))
print ("</ul>")
print ("</body>")
print ("</html>")