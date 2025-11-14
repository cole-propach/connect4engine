const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const path = require('path');

const app = express();
const PORT = 3000;

const exePath = path.join(__dirname, 'engine.exe');

app.use(cors());

//example URL: http://localhost:3000/run?arg1=hello&arg2=world
app.get('/run', (req, res) => {
    const { arg1, arg2 } = req.query;

    if (!arg1 || !arg2) {
        return res.status(400).send('Please provide both arg1 and arg2 in the query string.');
    }

    //run the exe with the arguments
    execFile(exePath, [arg1, arg2], (error, stdout, stderr) => {
        if (error) {
            console.error('Error:', error);
            return res.status(500).send(`Error: ${error.message}`);
        }
        if (stderr) {
            console.error('Stderr:', stderr);
        }

        res.send(`${stdout}`);
    });
});

app.listen(PORT, () => {
    console.log(`Server running on port ${PORT}`);
});
