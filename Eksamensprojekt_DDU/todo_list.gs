function doGet(e) {

  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

  var listName = e.parameter.list;   // fx "MyList"
  var task = e.parameter.task;       // fx "task8"

  var data = sheet.getDataRange().getValues();

  var listRow = -1;
  var listCol = -1;

  //Find hvilken række og kolonne listen er i
  for (var i = 1; i < data.length; i++) {
    if (data[i][0] == listName) {
      listRow = i + 1; // +1 fordi Sheets starter ved 1
      break;
    }
  }

  for (var j = 1; j < data[0].length; j++) {
    if (data[0][j].includes(listName)) {
      listCol = j + 1;
      break;
    }
  }

  // hvis listen ikke findes
  if (listRow == -1 || listCol == -1) {
    return ContentService.createTextOutput("List not found");
  }

  //Find første tomme celle i kolonnen
  var colValues = sheet.getRange(2, listCol, sheet.getLastRow()).getValues();

  var insertRow = 2;
  for (var i = 0; i < colValues.length; i++) {
    if (colValues[i][0] == "") {
      insertRow = i + 2;
      break;
    }
  }

  //indsæt task
  sheet.getRange(insertRow, listCol).setValue(task);

  return ContentService.createTextOutput("Added");
}
